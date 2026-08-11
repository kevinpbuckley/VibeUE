// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBehaviorTreeService.h"

#include "AIGraphTypes.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_SimpleParallel.h"
#include "BehaviorTreeGraphNode_SubtreeTask.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "BehaviorTreeServiceInternal.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"

/**
 * Structural writes to a Behavior Tree's editor graph.
 *
 * ===========================================================================================
 *  Why nothing here calls BreakAllNodeLinks / DestroyNode / RemoveNode(bBreakAllLinks = true)
 * ===========================================================================================
 *
 * UAIGraphNode::NodeConnectionListChanged() is not a deferred notification. It is:
 *
 *     void UAIGraphNode::NodeConnectionListChanged()
 *     {
 *         Super::NodeConnectionListChanged();
 *         GetAIGraph()->UpdateAsset();          // AIGraphNode.cpp:236-241
 *     }
 *
 * — a synchronous, immediate regeneration of UBehaviorTree::RootNode from whatever the graph looks
 * like at that instant. UEdGraphNode::BreakAllNodeLinks() fires it on every node that lost a link
 * (EdGraphNode.cpp:487-517), and UEdGraph::RemoveNode() calls BreakAllNodeLinks() by default
 * (EdGraph.cpp:261-281), as does UEdGraphNode::DestroyNode().
 *
 * That matters because the *intermediate* states of an edit are not valid trees. Unlink the root's
 * only child and the runtime tree is regenerated as empty right there, before this function has
 * finished, before CommitGraph runs, and outside the reach of CommitGraph's discard guard — which
 * only brackets its own OnSave() call and can neither see nor undo a rebuild that already happened.
 *
 * The fix is not to fight it. The engine's own pin-level API does exactly what is needed and
 * notifies nothing:
 *
 *   - UEdGraphPin::MakeLinkTo / BreakLinkTo / BreakAllPinLinks(bNotifyNodes = false) mutate the two
 *     LinkedTo arrays and stop (EdGraphPin.cpp:514, 661, 705). BreakAllNodeLinks itself uses them
 *     that way, passing bNotifyNodes = false, and then sends the notifications separately.
 *   - UEdGraph::AddNode and UEdGraph::RemoveNode(..., bBreakAllLinks = false) broadcast
 *     OnGraphChanged (an editor-UI signal with no listeners headlessly) and never touch UpdateAsset.
 *
 * So every function below rewires pins directly and lets CommitGraph's OnSave() perform the single
 * regeneration, once, over a graph that is valid again — with the discard guard watching. The graph
 * is never left in an invalid shape across a call: each function either completes its rewiring or
 * restores what it found.
 *
 * The one shape that cannot be made safe this way is emptying the tree, because there is no valid
 * end state — so RemoveNode refuses the root's only child up front rather than mutating and then
 * failing at commit.
 */

// Named, not anonymous: this module builds with unity/jumbo enabled, where two anonymous namespaces
// in the same blob collide on identical helper names (docs/gotchas.md).
namespace VibeBTEdit
{
	/** The node's child-carrying pin, or nullptr — task and decorator nodes have none. */
	UEdGraphPin* FindOutputPin(UBehaviorTreeGraphNode* Node)
	{
		if (Node)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Output)
				{
					return Pin;
				}
			}
		}
		return nullptr;
	}

	/** The node's parent-facing pin, or nullptr — the root node has none. */
	UEdGraphPin* FindInputPin(UBehaviorTreeGraphNode* Node)
	{
		if (Node)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Input)
				{
					return Pin;
				}
			}
		}
		return nullptr;
	}

	/** Name to put in an error message for a node whose class the caller would recognise. */
	FString DescribeNode(const UBehaviorTreeGraphNode* Node)
	{
		if (!Node)
		{
			return TEXT("<null>");
		}
		return Node->NodeInstance
			? Node->NodeInstance->GetClass()->GetName()
			: Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	}

	/**
	 * Move an existing link to Index in the parent's LinkedTo order, appending when Index < 0.
	 *
	 * This is what makes an insert position mean anything: UBehaviorTreeGraph::UpdateAsset walks the
	 * output pin's LinkedTo in order to build UBTCompositeNode::Children, and CommitGraph's
	 * ArrangeGraph pass has already converted that same order into the strictly increasing X
	 * positions that RebuildChildOrder re-sorts by (BehaviorTreeGraph.cpp:1189). Reordering LinkedTo
	 * alone, without the layout pass, would be undone the first time the graph was re-sorted.
	 */
	void PlaceLinkAt(UEdGraphPin* ParentOut, UEdGraphPin* ChildIn, int32 Index)
	{
		ParentOut->LinkedTo.Remove(ChildIn);
		const int32 Target = (Index < 0)
			? ParentOut->LinkedTo.Num()
			: FMath::Clamp(Index, 0, ParentOut->LinkedTo.Num());
		ParentOut->LinkedTo.Insert(ChildIn, Target);
	}

	/** Node and everything reachable below it, pre-order, cycle-guarded. */
	void CollectSubtree(UBehaviorTreeGraphNode* Node, TArray<UBehaviorTreeGraphNode*>& Out)
	{
		if (!Node || Out.Contains(Node))
		{
			return;
		}
		Out.Add(Node);
		for (UBehaviorTreeGraphNode* Child : VibeBT::GetChildNodes(Node))
		{
			CollectSubtree(Child, Out);
		}
	}

	/** Whether Candidate is Ancestor or sits somewhere below it. */
	bool IsSelfOrDescendant(UBehaviorTreeGraphNode* Candidate, UBehaviorTreeGraphNode* Ancestor)
	{
		TArray<UBehaviorTreeGraphNode*> Subtree;
		CollectSubtree(Ancestor, Subtree);
		return Subtree.Contains(Candidate);
	}

	/**
	 * Ask the BT schema whether ParentOut -> ChildIn is a legal link, and refuse anything short of
	 * an unconditional yes.
	 *
	 * Delegated rather than reimplemented because the rules are the schema's and one of them is
	 * load bearing to the point of being a crash: the root node's pin is PinCategory_SingleComposite
	 * (BehaviorTreeGraphNode_Root.cpp), and linking a task there makes
	 * UBehaviorTreeGraph::CreateBTFromGraph assign a null BTAsset->RootNode and then hand that null
	 * straight to BTGraphHelpers::CreateChildren, which dereferences it as RootNode->Children.Reset()
	 * behind a guard that only tests the *ed-graph* node (BehaviorTreeGraph.cpp:902-936, 515-518).
	 * On a tree that has no runtime root yet — a freshly created one — CommitGraph's discard guard
	 * has nothing to protect and lets it through, so this check is the only thing standing between
	 * AddNode(..., "Root", "BTTask_Wait") and an editor crash inside OnSave().
	 *
	 * The BREAK_OTHERS_* responses are refused too, not applied: they mean "this pin is exclusive
	 * and something else is already on it", i.e. adding a second child under the root. Honouring
	 * them would silently unlink the existing tree.
	 */
	FString CheckLink(const UBehaviorTreeGraph* Graph, UEdGraphPin* ParentOut, UEdGraphPin* ChildIn)
	{
		const UEdGraphSchema* Schema = Graph ? Graph->GetSchema() : nullptr;
		if (!Schema)
		{
			return TEXT("Behavior Tree graph has no schema");
		}

		const FPinConnectionResponse Response = Schema->CanCreateConnection(ParentOut, ChildIn);
		if (Response.Response == CONNECT_RESPONSE_MAKE)
		{
			return FString();
		}

		return FString::Printf(TEXT("%s cannot take %s as a child: %s"),
			*DescribeNode(Cast<UBehaviorTreeGraphNode>(ParentOut->GetOwningNode())),
			*DescribeNode(Cast<UBehaviorTreeGraphNode>(ChildIn->GetOwningNode())),
			*Response.Message.ToString());
	}

	/** The UBehaviorTreeGraphNode subclass the BT schema would spawn for this node class. */
	UClass* GraphNodeClassFor(UClass* NodeClass)
	{
		// Order matters: the two special cases are subclasses of the two general ones. Mirrors
		// UEdGraphSchema_BehaviorTree::GetGraphContextActions (EdGraphSchema_BehaviorTree.cpp:150-190).
		if (NodeClass->IsChildOf(UBTComposite_SimpleParallel::StaticClass()))
		{
			return UBehaviorTreeGraphNode_SimpleParallel::StaticClass();
		}
		if (NodeClass->IsChildOf(UBTCompositeNode::StaticClass()))
		{
			return UBehaviorTreeGraphNode_Composite::StaticClass();
		}
		if (NodeClass->IsChildOf(UBTTask_RunBehavior::StaticClass()))
		{
			return UBehaviorTreeGraphNode_SubtreeTask::StaticClass();
		}
		if (NodeClass->IsChildOf(UBTTaskNode::StaticClass()))
		{
			return UBehaviorTreeGraphNode_Task::StaticClass();
		}
		return nullptr;
	}
}

using namespace VibeBTEdit;

FString UBehaviorTreeService::AddNode(const FString& AssetPath, const FString& ParentNodePath,
	const FString& NodeClassName, int32 ChildIndex)
{
	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	const FString GuardError = VibeBT::OpenWriteGuard(AssetPath, Tree, Graph);
	if (!GuardError.IsEmpty())
	{
		return FString::Printf(TEXT("ERROR: %s"), *GuardError);
	}

	UBehaviorTreeGraphNode* Parent = VibeBT::ResolveNodePath(Graph, ParentNodePath);
	if (!Parent)
	{
		return FString::Printf(
			TEXT("ERROR: no node at path '%s' in %s (nothing there, or the path is ambiguous and "
				 "needs an index, as in 'Sequence[1]')"),
			*ParentNodePath, *AssetPath);
	}

	// Checked before anything is created, so a rejected add leaves no stray node behind.
	UEdGraphPin* ParentOut = FindOutputPin(Parent);
	if (!ParentOut)
	{
		return FString::Printf(TEXT("ERROR: %s cannot have children"), *DescribeNode(Parent));
	}

	UClass* NodeClass = VibeBT::ResolveNodeClass(NodeClassName, UBTNode::StaticClass());
	if (!NodeClass)
	{
		return FString::Printf(
			TEXT("ERROR: unknown or ambiguous Behavior Tree node class '%s'. Use "
				 "GetAvailableNodeTypes to list what is available."),
			*NodeClassName);
	}

	UClass* GraphNodeClass = GraphNodeClassFor(NodeClass);
	if (!GraphNodeClass)
	{
		return FString::Printf(
			TEXT("ERROR: %s is neither a composite nor a task. Decorators and services attach to a "
				 "node rather than being placed in the tree."),
			*NodeClass->GetName());
	}

	Graph->Modify();

	UBehaviorTreeGraphNode* NewNode = NewObject<UBehaviorTreeGraphNode>(Graph, GraphNodeClass,
		NAME_None, RF_Transactional);
	NewNode->ClassData = FGraphNodeClassData(NodeClass, FString());
	NewNode->CreateNewGuid();
	NewNode->PostPlacedNewNode();      // spawns NodeInstance, outered to the UBehaviorTree
	NewNode->AllocateDefaultPins();
	Graph->AddNode(NewNode, /*bFromUI*/ false, /*bSelectNewNode*/ false);

	// From here on, any failure has to take the node back out. bBreakAllLinks = false throughout:
	// it is unlinked anyway, and the true path would fire UpdateAsset (see the file header).
	UEdGraphPin* NewIn = FindInputPin(NewNode);
	if (!NewIn)
	{
		Graph->RemoveNode(NewNode, /*bBreakAllLinks*/ false);
		return FString::Printf(TEXT("ERROR: %s has no input pin and cannot be placed in a tree"),
			*NodeClass->GetName());
	}

	if (!NewNode->NodeInstance)
	{
		// PostPlacedNewNode silently leaves NodeInstance null if the class failed to load. Letting
		// that reach the graph is what CommitGraph's null-instance guard exists to catch; catching
		// it here says which node, and costs the asset nothing.
		Graph->RemoveNode(NewNode, /*bBreakAllLinks*/ false);
		return FString::Printf(TEXT("ERROR: failed to create a node instance of %s"),
			*NodeClass->GetName());
	}

	const FString LinkError = CheckLink(Graph, ParentOut, NewIn);
	if (!LinkError.IsEmpty())
	{
		Graph->RemoveNode(NewNode, /*bBreakAllLinks*/ false);
		return FString::Printf(TEXT("ERROR: %s"), *LinkError);
	}

	Parent->Modify();
	ParentOut->MakeLinkTo(NewIn);
	PlaceLinkAt(ParentOut, NewIn, ChildIndex);

	const FString CommitError = VibeBT::CommitGraph(Tree, Graph);
	if (!CommitError.IsEmpty())
	{
		return FString::Printf(TEXT("ERROR: %s"), *CommitError);
	}

	return VibeBT::GetNodePath(NewNode);
}

FString UBehaviorTreeService::RemoveNode(const FString& AssetPath, const FString& NodePath)
{
	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	const FString GuardError = VibeBT::OpenWriteGuard(AssetPath, Tree, Graph);
	if (!GuardError.IsEmpty())
	{
		return GuardError;
	}

	UBehaviorTreeGraphNode* Node = VibeBT::ResolveNodePath(Graph, NodePath);
	if (!Node)
	{
		return FString::Printf(
			TEXT("No node at path '%s' in %s (nothing there, or the path is ambiguous and needs an "
				 "index, as in 'Sequence[1]')"),
			*NodePath, *AssetPath);
	}

	if (Node->IsA<UBehaviorTreeGraphNode_Root>())
	{
		return TEXT("The root node cannot be removed: it is the graph's entry point, and every path "
					"is expressed relative to it.");
	}

	UBehaviorTreeGraphNode* Parent = VibeBT::GetParentNode(Node);
	if (!Parent)
	{
		return FString::Printf(
			TEXT("'%s' has no parent, so it is not part of the tree and cannot be removed from it"),
			*NodePath);
	}

	if (Parent->IsA<UBehaviorTreeGraphNode_Root>())
	{
		// The root takes exactly one child, so this is a request to empty the tree. Refused here,
		// before anything is mutated, rather than at commit: CommitGraph would reject the resulting
		// graph anyway (a root that leads nowhere over a populated runtime tree), leaving this
		// in-memory copy edited and unsaved for no gain.
		return FString::Printf(
			TEXT("'%s' is the tree's only top-level node; removing it would leave %s with no runtime "
				 "tree at all, which CommitGraph refuses to save. Remove its children instead, or "
				 "replace the asset."),
			*NodePath, *AssetPath);
	}

	// The whole subtree, not just the one node. A child left dangling would be unreachable from
	// "Root", so no path could name it again — it would simply become invisible weight in the saved
	// asset, still counted by GetBehaviorTreeInfo and still loaded with it.
	TArray<UBehaviorTreeGraphNode*> Doomed;
	CollectSubtree(Node, Doomed);

	Graph->Modify();
	Parent->Modify();
	for (UBehaviorTreeGraphNode* Dying : Doomed)
	{
		Dying->Modify();
		for (UEdGraphPin* Pin : Dying->Pins)
		{
			if (Pin)
			{
				Pin->BreakAllPinLinks(/*bNotifyNodes*/ false);
			}
		}
		Graph->RemoveNode(Dying, /*bBreakAllLinks*/ false);
	}

	return VibeBT::CommitGraph(Tree, Graph);
}

FString UBehaviorTreeService::MoveNode(const FString& AssetPath, const FString& NodePath,
	const FString& NewParentPath, int32 NewChildIndex)
{
	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	const FString GuardError = VibeBT::OpenWriteGuard(AssetPath, Tree, Graph);
	if (!GuardError.IsEmpty())
	{
		return GuardError;
	}

	UBehaviorTreeGraphNode* Node = VibeBT::ResolveNodePath(Graph, NodePath);
	if (!Node)
	{
		return FString::Printf(
			TEXT("No node at path '%s' in %s (nothing there, or the path is ambiguous and needs an "
				 "index, as in 'Sequence[1]')"),
			*NodePath, *AssetPath);
	}
	if (Node->IsA<UBehaviorTreeGraphNode_Root>())
	{
		return TEXT("The root node cannot be moved: it is the graph's entry point.");
	}

	UBehaviorTreeGraphNode* NewParent = VibeBT::ResolveNodePath(Graph, NewParentPath);
	if (!NewParent)
	{
		return FString::Printf(
			TEXT("No node at new-parent path '%s' in %s (nothing there, or the path is ambiguous and "
				 "needs an index, as in 'Sequence[1]')"),
			*NewParentPath, *AssetPath);
	}

	if (IsSelfOrDescendant(NewParent, Node))
	{
		// Allowing it would detach the whole subtree from the root into a free-floating ring, which
		// UpdateAsset would then drop from the runtime tree entirely.
		return FString::Printf(
			TEXT("Cannot move '%s' under '%s': that is the node itself, or one of its own "
				 "descendants, and the result would not be a tree."),
			*NodePath, *NewParentPath);
	}

	UEdGraphPin* NewParentOut = FindOutputPin(NewParent);
	if (!NewParentOut)
	{
		return FString::Printf(TEXT("%s cannot have children"), *DescribeNode(NewParent));
	}

	UEdGraphPin* NodeIn = FindInputPin(Node);
	UBehaviorTreeGraphNode* OldParent = VibeBT::GetParentNode(Node);
	if (!NodeIn || !OldParent)
	{
		return FString::Printf(
			TEXT("'%s' has no parent, so it is not part of the tree and cannot be moved within it"),
			*NodePath);
	}

	Graph->Modify();
	Node->Modify();
	NewParent->Modify();

	if (OldParent == NewParent)
	{
		// A reorder under the same parent. Deliberately done without unlinking anything: there is
		// no intermediate state to get wrong, and the link the schema would be asked about already
		// exists (it would answer BREAK_OTHERS, not MAKE).
		PlaceLinkAt(NewParentOut, NodeIn, NewChildIndex);
	}
	else
	{
		UEdGraphPin* OldParentOut = FindOutputPin(OldParent);
		if (!OldParentOut)
		{
			return FString::Printf(TEXT("'%s' is not linked through its parent's output pin"), *NodePath);
		}
		const int32 OldIndex = OldParentOut->LinkedTo.IndexOfByKey(NodeIn);

		// Unlinked first, because CanCreateConnection answers BREAK_OTHERS_* — not MAKE — for a
		// child that still has a parent, and this code refuses anything but MAKE. The window is
		// safe: no notification fires (see the file header), so nothing regenerates the runtime
		// tree while the node is detached, and a refusal below puts it back exactly where it was.
		OldParent->Modify();
		OldParentOut->BreakLinkTo(NodeIn);

		const FString LinkError = CheckLink(Graph, NewParentOut, NodeIn);
		if (!LinkError.IsEmpty())
		{
			OldParentOut->MakeLinkTo(NodeIn);
			PlaceLinkAt(OldParentOut, NodeIn, OldIndex);
			return LinkError;
		}

		NewParentOut->MakeLinkTo(NodeIn);
		PlaceLinkAt(NewParentOut, NodeIn, NewChildIndex);
	}

	return VibeBT::CommitGraph(Tree, Graph);
}
