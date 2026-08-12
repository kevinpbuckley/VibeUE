// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBehaviorTreeService.h"

#include "AIGraphTypes.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "BehaviorTreeGraphNode_SimpleParallel.h"
#include "BehaviorTreeGraphNode_SubtreeTask.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "BehaviorTreeServiceInternal.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Misc/ScopeExit.h"
#include "Misc/StringOutputDevice.h"
#include "UObject/UnrealType.h"

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
 *
 * ===========================================================================================
 *  ...and why the sub-node functions do not call UAIGraphNode::AddSubNode
 * ===========================================================================================
 *
 * Same reason, one level up. UAIGraphNode::AddSubNode is not a data operation; it ends with
 *
 *     ParentGraph->NotifyGraphChanged();
 *     GetAIGraph()->UpdateAsset();          // AIGraphNode.cpp:297-298
 *
 * so attaching a decorator through it regenerates UBehaviorTree::RootNode on the spot — before
 * CommitGraph runs, outside the discard guard, and (for AddDecorator's own failure paths) before
 * this code has decided whether the attach is going to stand at all. It also opens an FScopedTransaction
 * and calls AutowireNewNode, neither of which belongs in a headless batch edit.
 *
 * What it does that matters is all data: Rename the sub-node under the graph, set ParentNode, give it
 * a GUID, run PostPlacedNewNode/AllocateDefaultPins, push it onto SubNodes, and hand it to
 * OnSubNodeAdded. AttachSubNode below does exactly that list, minus the notifications, and creates the
 * node with NewObject<>(Graph, ...) so the Rename is unnecessary to begin with.
 *
 * The two neighbouring engine helpers are safe but not used either:
 *
 *   - UBehaviorTreeGraphNode::OnSubNodeAdded (BehaviorTreeGraphNode.cpp:228-254) is pure data and
 *     notifies nothing — but it always appends (ahead of the first injected decorator), so it cannot
 *     honour an explicit Index. AttachSubNode reproduces its injected-node rule and adds the index.
 *   - UBehaviorTreeGraphNode::InsertSubNodeAt (:290-345) does take an index, but a single int32 with
 *     three +1-encoded byte fields packed into it, decoded per array. Passing an ordinary decorator
 *     index to it silently means something else.
 *
 * Both halves of the attach are mandatory, and neither is redundant:
 *
 *   - Decorators / Services is what CreateBTFromGraph reads to build the runtime tree.
 *   - SubNodes is what UAIGraph::CollectAllNodeInstances walks (AIGraph.cpp:188-205). A sub-node
 *     missing from it has its UBTDecorator/UBTService instance treated as an orphan by
 *     RemoveOrphanedNodes — which runs at the end of every CreateBTFromGraph — and renamed into the
 *     transient package, i.e. the decorator this call just added is destroyed by the commit that was
 *     supposed to save it.
 *
 * Sub-nodes are NOT added to UEdGraph::Nodes. That is not a stylistic match with the engine
 * (BTGraphHelpers::SpawnMissingDecoratorNodes, BehaviorTreeGraph.cpp:775-810) — it is mandatory,
 * because UpdateAsset's own first pass would break a sub-node that was in there:
 *
 *     for (int32 Index = 0; Index < Nodes.Num(); ++Index)   // BehaviorTreeGraph.cpp:111-146
 *     {
 *         ...
 *         Node->ParentNode = NULL;                          // :136 — unconditionally, per entry
 *         for (...) Node->Services[iAux]->ParentNode = Node;
 *         for (...) Node->Decorators[iAux]->ParentNode = Node;
 *     }
 *
 * A sub-node listed in Nodes gets its own ParentNode nulled when its own iteration comes round, and
 * only the owner's Decorators/Services loop puts it back — so whether it survives depends entirely
 * on whether the owner happens to sit later in Nodes. When it does not, GetNodePath returns "" (no
 * owner to build an "@decorator[n]" segment from) and RemoveSubNode misreports the node as "not a
 * decorator or service". Order-dependent, silent, and only on some assets.
 *
 * CountGraphNodes would also double-count (it adds Nodes.Num() and then walks SubNodes). By
 * contrast FindRootGraphNode and ArrangeGraph would NOT break: the former casts each entry to
 * UBehaviorTreeGraphNode_Root, which a decorator never matches, and the latter walks down from the
 * root through pins, which never reach a pinless sub-node.
 */

// Named, not anonymous: this module builds with unity/jumbo enabled, where two anonymous namespaces
// in the same blob collide on identical helper names — the second definition silently wins and the
// other translation unit calls it instead of its own.
namespace VibeBTEdit
{
	/**
	 * The three name shapes the path grammar cannot express, refused wherever a node name can be
	 * written — SetNodeName, and SetNodePropertyValue on UBTNode::NodeName, which is the same write
	 * through reflection (and the route BuildTree replays a dump through). Checking only one of the
	 * two would leave the other a bypass that renames a node into un-addressability.
	 */
	FString CheckNodeNameGrammar(const FString& NewName)
	{
		if (NewName.Contains(TEXT("/")))
		{
			return FString::Printf(
				TEXT("Node names cannot contain '/': it is the path separator, so '%s' would leave the "
					 "node un-addressable."),
				*NewName);
		}
		if (NewName.StartsWith(TEXT("@")))
		{
			// ResolveNodePath dispatches on the leading '@' before it ever compares names: a segment
			// starting with one is looked up in Decorators/Services by index, matches neither
			// "@decorator" nor "@service", and returns nullptr. A node named "@foo" therefore has a
			// path that GetNodePath will happily emit and nothing can resolve.
			return FString::Printf(
				TEXT("Node names cannot start with '@': the path grammar reserves that prefix for "
					 "sub-node slots (@decorator[n], @service[n]), so '%s' would leave the node "
					 "un-addressable."),
				*NewName);
		}
		if (NewName.EndsWith(TEXT("]")))
		{
			int32 OpenIdx = INDEX_NONE;
			if (NewName.FindLastChar(TEXT('['), OpenIdx))
			{
				const FString IndexText = NewName.Mid(OpenIdx + 1, NewName.Len() - OpenIdx - 2);
				if (!IndexText.IsEmpty() && IndexText.IsNumeric())
				{
					return FString::Printf(
						TEXT("Node names cannot end in a numeric '[n]': the path grammar reads that as "
							 "a sibling index, so '%s' would resolve to something else."),
						*NewName);
				}
			}
		}
		return FString();
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
	 * Every pin this node can hang children on, in Pins order.
	 *
	 * Deliberately a list, not a pin. Most BT graph nodes have one output pin, but
	 * UBehaviorTreeGraphNode_SimpleParallel has two — [0] "Task" (PinCategory_SingleTask, the main
	 * task) and [1] "Out" (PinCategory_SingleNode, the background branch)
	 * (BehaviorTreeGraphNode_SimpleParallel.cpp:24-30). Anything that reaches for "the" output pin
	 * silently means "the main task pin" on a parallel, which is how a break of a background link
	 * becomes a no-op and a restore becomes a fabricated second parent link.
	 *
	 * Pins order is also child order everywhere else in this service: VibeBT::GetChildNodes walks it,
	 * and so does BTGraphHelpers::CreateChildren when it builds UBTCompositeNode::Children
	 * (BehaviorTreeGraph.cpp:540-560). So slot index == the index GetTree reports == the runtime
	 * child index.
	 */
	TArray<UEdGraphPin*> ChildSlotPins(const UBehaviorTreeGraphNode* Node)
	{
		TArray<UEdGraphPin*> Slots;
		if (Node)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Output)
				{
					Slots.Add(Pin);
				}
			}
		}
		return Slots;
	}

	/**
	 * Whether this pin holds an ordered list of children rather than exactly one.
	 *
	 * Compared against the literal rather than UBehaviorTreeEditorTypes::PinCategory_MultipleNodes:
	 * those statics are declared without an export macro, so referencing one from another module
	 * compiles and then fails to link. The literal is the value the engine constructs
	 * (BehaviorTreeEditorTypes.cpp:9-12) and the one serialised into every Behavior Tree asset, so
	 * it is as stable as the symbol would have been.
	 */
	bool IsMultiChildPin(const UEdGraphPin* Pin)
	{
		static const FName MultipleNodesCategory(TEXT("MultipleNodes"));
		return Pin && Pin->PinType.PinCategory == MultipleNodesCategory;
	}

	/**
	 * The parent output pin ChildIn is actually linked through — read off the link itself rather
	 * than guessed from the parent's pin list. A BT node has exactly one parent, so LinkedTo[0] is
	 * the whole answer, and it is right on a SimpleParallel background child where picking the
	 * first output pin is not.
	 */
	UEdGraphPin* LinkingParentPin(const UEdGraphPin* ChildIn)
	{
		return (ChildIn && ChildIn->LinkedTo.Num() > 0) ? ChildIn->LinkedTo[0] : nullptr;
	}

	/** Where a child is going: which pin, and where within that pin's link order. */
	struct FChildSlot
	{
		UEdGraphPin* Pin = nullptr;

		/** Position within Pin->LinkedTo; < 0 appends. Always 0 for a single-child pin. */
		int32 InsertPosition = -1;

		/** True when ChildIndex selected the pin itself, i.e. the parent has fixed child slots. */
		bool bFixedSlots = false;
	};

	/**
	 * Work out which pin a ChildIndex refers to on this parent, and where in it.
	 *
	 * Two shapes, and the difference is the pin category rather than the node class:
	 *
	 *  - An ordinary composite has one PinCategory_MultipleNodes pin holding an ordered list, so
	 *    ChildIndex is a position within it and < 0 appends.
	 *  - A parent whose pins each take exactly one child — the root (SingleComposite) and
	 *    SimpleParallel (SingleTask + SingleNode) — has fixed slots, so ChildIndex picks the pin and
	 *    < 0 means "the first free one". This is the same numbering GetTree reports, because both
	 *    walk Pins in order.
	 */
	FString ResolveChildSlot(UBehaviorTreeGraphNode* Parent, int32 ChildIndex, FChildSlot& Out)
	{
		const TArray<UEdGraphPin*> Slots = ChildSlotPins(Parent);
		if (Slots.Num() == 0)
		{
			return FString::Printf(TEXT("%s cannot have children"), *DescribeNode(Parent));
		}

		if (Slots.Num() == 1 && IsMultiChildPin(Slots[0]))
		{
			Out.Pin = Slots[0];
			Out.InsertPosition = ChildIndex;
			Out.bFixedSlots = false;
			return FString();
		}

		Out.bFixedSlots = true;
		Out.InsertPosition = 0;

		if (ChildIndex >= Slots.Num())
		{
			return FString::Printf(
				TEXT("%s has %d child slot(s), so child index %d is out of range"),
				*DescribeNode(Parent), Slots.Num(), ChildIndex);
		}
		if (ChildIndex >= 0)
		{
			Out.Pin = Slots[ChildIndex];
			return FString();
		}

		for (UEdGraphPin* Slot : Slots)
		{
			if (Slot->LinkedTo.Num() == 0)
			{
				Out.Pin = Slot;
				return FString();
			}
		}

		return FString::Printf(
			TEXT("%s has no free child slot (all %d in use). Its children occupy fixed slots rather "
				 "than an open list, so pass an explicit child index to name the one you mean."),
			*DescribeNode(Parent), Slots.Num());
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

	/**
	 * Unlink and delete Node and everything under it, pin-level throughout.
	 *
	 * Pin->BreakAllPinLinks(bNotifyNodes = false) and RemoveNode(bBreakAllLinks = false) are the
	 * non-notifying halves of the engine's own removal; see the file header for why the notifying
	 * ones must not be used here.
	 */
	void DetachAndRemoveSubtree(UBehaviorTreeGraph* Graph, UBehaviorTreeGraphNode* Node)
	{
		TArray<UBehaviorTreeGraphNode*> Doomed;
		CollectSubtree(Node, Doomed);

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
	 *
	 * bAllowOccupied is the one exception, and it is narrow: BREAK_OTHERS_A means precisely "pin A
	 * is single-link and already has something on it", which is the deliberate replace of a
	 * SimpleParallel slot. It cannot mask a type error — CanCreateConnection tests the
	 * SingleComposite / SingleTask categories first and returns DISALLOW for those
	 * (EdGraphSchema_BehaviorTree.cpp:260-268), long before it reaches the exclusivity branches
	 * (:339-372). Callers passing true must still remove the displaced node themselves, and only
	 * after this has said yes.
	 */
	FString CheckLink(const UBehaviorTreeGraph* Graph, UEdGraphPin* ParentOut, UEdGraphPin* ChildIn,
		bool bAllowOccupied)
	{
		const UEdGraphSchema* Schema = Graph ? Graph->GetSchema() : nullptr;
		if (!Schema)
		{
			return TEXT("Behavior Tree graph has no schema");
		}

		const FPinConnectionResponse Response = Schema->CanCreateConnection(ParentOut, ChildIn);
		if (Response.Response == CONNECT_RESPONSE_MAKE
			|| (bAllowOccupied && Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_A))
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

	/** Which of a node's two sub-node arrays a call is about. */
	enum class ESubNodeKind : uint8
	{
		Decorator,
		Service
	};

	/** The owner's array for this kind. Never null — every UBehaviorTreeGraphNode has both. */
	TArray<TObjectPtr<UBehaviorTreeGraphNode>>& SlotArray(UBehaviorTreeGraphNode* Owner, ESubNodeKind Kind)
	{
		return (Kind == ESubNodeKind::Decorator) ? Owner->Decorators : Owner->Services;
	}

	/** "decorator" / "service", for error messages and the path segment. */
	const TCHAR* SubNodeKindName(ESubNodeKind Kind)
	{
		return (Kind == ESubNodeKind::Decorator) ? TEXT("decorator") : TEXT("service");
	}

	/**
	 * Whether Node's sub-nodes reach the runtime tree at all, and why not when they do not.
	 *
	 * The rule is not "which graph node class is it" but "what does CreateBTFromGraph read". It reads
	 * decorators and services off exactly two shapes: the node linked under the graph root (whose
	 * decorators become UBehaviorTree::RootDecorators and whose services become the root composite's,
	 * BehaviorTreeGraph.cpp:917-936 and 518-535), and each child graph node whose NodeInstance casts
	 * to UBTCompositeNode or UBTTaskNode (:568-627). Anything else it skips, with a bare `continue`.
	 *
	 * So the test is the instance, not the class of the ed-graph node, and it catches three distinct
	 * mistakes at once:
	 *
	 *  - The graph's Root node (UBehaviorTreeGraphNode_Root) carries no instance. Its OWN Decorators
	 *    array is never read by anything: root-level decorators live on the top composite, one level
	 *    down. A decorator attached to "Root" would sit in the saved asset forever and never run.
	 *  - A decorator or service carries a UBTDecorator / UBTService instance, which is neither, so a
	 *    sub-node of a sub-node is dropped.
	 *  - A node whose instance failed to load carries null, and is skipped along with everything
	 *    attached to it.
	 *
	 * Returns an empty string when Node can carry sub-nodes.
	 */
	FString CheckCanCarrySubNodes(UBehaviorTreeGraphNode* Node)
	{
		if (Node->IsA<UBehaviorTreeGraphNode_Root>())
		{
			return TEXT("the graph's Root node cannot carry decorators or services. Root-level "
						"decorators belong on the tree's top composite (the node directly under Root): "
						"that is where UBehaviorTreeGraph::CreateBTFromGraph reads "
						"UBehaviorTree::RootDecorators from. One attached to Root itself would be saved "
						"and never run.");
		}

		if (Node->IsSubNode())
		{
			return FString::Printf(
				TEXT("%s is a decorator or service. Sub-nodes carry no sub-nodes of their own — attach "
					 "to the node they are on instead."),
				*DescribeNode(Node));
		}

		const UObject* Instance = ToRawPtr(Node->NodeInstance);
		if (!Cast<UBTCompositeNode>(Instance) && !Cast<UBTTaskNode>(Instance))
		{
			return FString::Printf(
				TEXT("%s carries no composite or task instance, so nothing attached to it would reach "
					 "the runtime tree"),
				*DescribeNode(Node));
		}

		return FString();
	}

	/**
	 * Where an authored decorator may be inserted, given Index.
	 *
	 * Authored decorators always precede injected ones. That ordering is the engine's, not a
	 * preference: UBehaviorTreeGraphNode::OnSubNodeAdded inserts before the first injected decorator
	 * (BehaviorTreeGraphNode.cpp:232-249), CollectDecorators skips injected entries outright when
	 * building the runtime tree (BehaviorTreeGraph.cpp:337-341), and
	 * UBehaviorTreeGraphNode_SubtreeTask::UpdateInjectedNodes strips every injected decorator and
	 * re-appends it on each refresh — so an authored decorator placed after one does not stay there.
	 * Clamping here means the returned "@decorator[n]" path is the position the node actually keeps.
	 *
	 * Services have no injected counterpart (UpdateInjectedNodes only touches Decorators), so their
	 * limit is simply the array length.
	 */
	int32 ClampSubNodeIndex(const TArray<TObjectPtr<UBehaviorTreeGraphNode>>& Slots, int32 Index,
		ESubNodeKind Kind)
	{
		int32 Limit = Slots.Num();
		if (Kind == ESubNodeKind::Decorator)
		{
			for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
			{
				const UBehaviorTreeGraphNode* Slot = ToRawPtr(Slots[SlotIndex]);
				if (Slot && Slot->bInjectedNode)
				{
					Limit = SlotIndex;
					break;
				}
			}
		}
		return (Index < 0) ? Limit : FMath::Clamp(Index, 0, Limit);
	}

	/**
	 * Create a sub-node of NodeClass and attach it to Owner at Index. Returns it, or null with
	 * OutError set — in which case nothing was attached and nothing needs unwinding.
	 *
	 * The engine's UAIGraphNode::AddSubNode is deliberately not used; see the file header.
	 */
	UBehaviorTreeGraphNode* AttachSubNode(UBehaviorTreeGraph* Graph, UBehaviorTreeGraphNode* Owner,
		UClass* NodeClass, ESubNodeKind Kind, int32 Index, FString& OutError)
	{
		UClass* const GraphNodeClass = (Kind == ESubNodeKind::Decorator)
			? UBehaviorTreeGraphNode_Decorator::StaticClass()
			: UBehaviorTreeGraphNode_Service::StaticClass();

		// Outered to the graph from the start, which is both what AddSubNode's Rename() achieves and
		// what UAIGraphNode::PostPlacedNewNode needs: it reaches the owning UBehaviorTree through
		// GetGraph()->GetOuter() to outer the node instance there (AIGraphNode.cpp:32-49).
		UBehaviorTreeGraphNode* SubNode =
			NewObject<UBehaviorTreeGraphNode>(Graph, GraphNodeClass, NAME_None, RF_Transactional);
		SubNode->ClassData = FGraphNodeClassData(NodeClass, FString());
		SubNode->CreateNewGuid();
		SubNode->PostPlacedNewNode();
		SubNode->AllocateDefaultPins();     // decorators and services define this as "no pins"

		if (!SubNode->NodeInstance)
		{
			// PostPlacedNewNode leaves NodeInstance null if the class failed to load. The sub-node is
			// not attached to anything yet, so abandoning it here leaves the graph untouched.
			OutError = FString::Printf(TEXT("failed to create a node instance of %s"),
				*NodeClass->GetName());
			return nullptr;
		}

		Graph->Modify();
		Owner->Modify();

		TArray<TObjectPtr<UBehaviorTreeGraphNode>>& Slots = SlotArray(Owner, Kind);
		SubNode->ParentNode = Owner;
		Owner->SubNodes.Add(SubNode);
		Slots.Insert(SubNode, ClampSubNodeIndex(Slots, Index, Kind));

		return SubNode;
	}

	/** Owner and slot array of a sub-node, or an error explaining why it is not one. */
	FString ResolveSubNodeSlot(UBehaviorTreeGraphNode* SubNode, const FString& SubNodePath,
		UBehaviorTreeGraphNode*& OutOwner, ESubNodeKind& OutKind, int32& OutIndex)
	{
		// VibeBT::GetSubNodeOwner, not SubNode->ParentNode: that pointer is UPROPERTY(transient) and
		// is null on every sub-node of a freshly loaded asset, which would make this report a real
		// production decorator as "a node in the tree".
		OutOwner = VibeBT::GetSubNodeOwner(SubNode);
		if (!OutOwner)
		{
			return FString::Printf(
				TEXT("'%s' is not a decorator or service — it is a node in the tree. Use RemoveNode."),
				*SubNodePath);
		}

		OutIndex = OutOwner->Decorators.IndexOfByKey(SubNode);
		if (OutIndex != INDEX_NONE)
		{
			OutKind = ESubNodeKind::Decorator;
			return FString();
		}

		OutIndex = OutOwner->Services.IndexOfByKey(SubNode);
		if (OutIndex != INDEX_NONE)
		{
			OutKind = ESubNodeKind::Service;
			return FString();
		}

		// Parented but in neither array. GetNodePath refuses to issue a path for this shape, so it is
		// not reachable through a path this service handed out — but a hand-written one could name it.
		return FString::Printf(
			TEXT("'%s' has an owner but appears in neither its decorator nor its service list, so there "
				 "is no slot to remove it from"),
			*SubNodePath);
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
	FChildSlot Slot;
	const FString SlotError = ResolveChildSlot(Parent, ChildIndex, Slot);
	if (!SlotError.IsEmpty())
	{
		return FString::Printf(TEXT("ERROR: %s"), *SlotError);
	}

	// A fixed slot that is already taken. On a SimpleParallel an explicit index means "put it
	// here", i.e. replace what is there — the only way to author a background branch at all, since
	// UBehaviorTreeGraph::SpawnMissingNodesForParallel refills an empty background pin with a
	// BTTask_Wait on every single save (BehaviorTreeGraph.cpp:1015-1074), so it is never free for a
	// later call to fill. Everywhere else — in practice the root — an occupied slot is refused:
	// replacing the tree's top composite is not something an AddNode should do by implication.
	UBehaviorTreeGraphNode* Displaced = nullptr;
	if (Slot.bFixedSlots && Slot.Pin->LinkedTo.Num() > 0)
	{
		UBehaviorTreeGraphNode* Occupant =
			Cast<UBehaviorTreeGraphNode>(Slot.Pin->LinkedTo[0]->GetOwningNode());
		if (!Parent->IsA<UBehaviorTreeGraphNode_SimpleParallel>())
		{
			return FString::Printf(
				TEXT("ERROR: child slot %d of %s is already taken by %s, and this node's slots are "
					 "not replaceable through AddNode"),
				ChildIndex, *DescribeNode(Parent), *DescribeNode(Occupant));
		}
		Displaced = Occupant;
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
	UEdGraphPin* NewIn = NewNode->GetInputPin();
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

	// Asked while the displaced node is still linked, and answered before it is removed: a type
	// violation still comes back DISALLOW here, so a refused replace destroys nothing.
	const FString LinkError = CheckLink(Graph, Slot.Pin, NewIn, /*bAllowOccupied*/ Displaced != nullptr);
	if (!LinkError.IsEmpty())
	{
		Graph->RemoveNode(NewNode, /*bBreakAllLinks*/ false);
		return FString::Printf(TEXT("ERROR: %s"), *LinkError);
	}

	Parent->Modify();
	if (Displaced)
	{
		DetachAndRemoveSubtree(Graph, Displaced);
	}
	Slot.Pin->MakeLinkTo(NewIn);
	PlaceLinkAt(Slot.Pin, NewIn, Slot.InsertPosition);

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

	if (Node->IsSubNode())
	{
		// Redirected by kind, not left to the pin walk below: a sub-node has no pins, so without
		// this it falls into "has no parent, so it is not part of the tree" — true of the pins and
		// useless to a caller holding a perfectly good decorator path.
		return FString::Printf(
			TEXT("'%s' is a decorator or service, not a node in the tree. Use RemoveSubNode."),
			*NodePath);
	}

	// Read off the link, not guessed from the parent's pin list — see LinkingParentPin.
	UEdGraphPin* NodeIn = Node->GetInputPin();
	UEdGraphPin* ParentPin = LinkingParentPin(NodeIn);
	UBehaviorTreeGraphNode* Parent =
		ParentPin ? Cast<UBehaviorTreeGraphNode>(ParentPin->GetOwningNode()) : nullptr;
	if (!Parent)
	{
		return FString::Printf(
			TEXT("'%s' has no parent, so it is not part of the tree and cannot be removed from it"),
			*NodePath);
	}

	if (Parent->IsA<UBehaviorTreeGraphNode_SimpleParallel>() && ParentPin == Parent->GetOutputPin(1))
	{
		// A parallel's background slot is never empty for long: SpawnMissingNodesForParallel runs
		// inside the very OnSave() this removal would commit through and links a fresh BTTask_Wait
		// onto an empty background pin (BehaviorTreeGraph.cpp:1015-1074). Removing it would report
		// success and leave a different node in its place, which is worse than refusing.
		return FString::Printf(
			TEXT("'%s' is the background branch of a SimpleParallel, which the engine regenerates as "
				 "a Wait task whenever the asset is saved, so it cannot be removed — only replaced. "
				 "Use AddNode(<parallel path>, <class>, 1)."),
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
	Graph->Modify();
	Parent->Modify();
	DetachAndRemoveSubtree(Graph, Node);

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

	if (Node->IsSubNode())
	{
		return FString::Printf(
			TEXT("'%s' is a decorator or service, not a node in the tree. Sub-nodes are not moved: "
				 "remove it with RemoveSubNode and re-add it on the other node."),
			*NodePath);
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

	FChildSlot Slot;
	const FString SlotError = ResolveChildSlot(NewParent, NewChildIndex, Slot);
	if (!SlotError.IsEmpty())
	{
		return SlotError;
	}

	// Both the pin and the parent come off the link itself. Deriving the pin from the parent's pin
	// list instead is wrong on a SimpleParallel background child, where it names the main-task pin:
	// the unlink below would then be a silent no-op and the restore would fabricate a second parent
	// link, leaving the node reachable from two pins and duplicated in UBTCompositeNode::Children.
	UEdGraphPin* NodeIn = Node->GetInputPin();
	UEdGraphPin* OldParentPin = LinkingParentPin(NodeIn);
	UBehaviorTreeGraphNode* OldParent =
		OldParentPin ? Cast<UBehaviorTreeGraphNode>(OldParentPin->GetOwningNode()) : nullptr;
	if (!NodeIn || !OldParentPin || !OldParent)
	{
		return FString::Printf(
			TEXT("'%s' has no parent, so it is not part of the tree and cannot be moved within it"),
			*NodePath);
	}

	// The same guard RemoveNode applies, for the same reason: moving the background branch away
	// leaves that pin empty, and SpawnMissingNodesForParallel refills it with a BTTask_Wait inside
	// the very commit this move saves through (BehaviorTreeGraph.cpp:1015-1074) — so the call would
	// report success and leave a node in the tree the caller never asked for. Scoped to moves that
	// actually LEAVE the pin: same-pin "moves" are no-ops on a single-link pin and harmless.
	if (OldParent->IsA<UBehaviorTreeGraphNode_SimpleParallel>()
		&& OldParentPin == OldParent->GetOutputPin(1)
		&& OldParentPin != Slot.Pin)
	{
		return FString::Printf(
			TEXT("'%s' is the background branch of a SimpleParallel, which the engine regenerates as "
				 "a Wait task whenever the asset is saved, so moving it away would silently leave a "
				 "new Wait in its place. Replace it instead: AddNode(<parallel path>, <class>, 1)."),
			*NodePath);
	}

	Graph->Modify();
	Node->Modify();
	NewParent->Modify();

	if (OldParentPin == Slot.Pin)
	{
		// A reorder within the same pin. Deliberately done without unlinking anything: there is no
		// intermediate state to get wrong, and the link the schema would be asked about already
		// exists (it would answer BREAK_OTHERS, not MAKE). Compared by pin rather than by parent so
		// that moving between a SimpleParallel's two pins takes the relink path below, as it must.
		PlaceLinkAt(Slot.Pin, NodeIn, Slot.InsertPosition);
	}
	else
	{
		if (Slot.bFixedSlots && Slot.Pin->LinkedTo.Num() > 0)
		{
			return FString::Printf(
				TEXT("Cannot move '%s' into child slot %d of '%s': that slot is already taken by %s. "
					 "MoveNode never displaces a node."),
				*NodePath, NewChildIndex, *NewParentPath,
				*DescribeNode(Cast<UBehaviorTreeGraphNode>(Slot.Pin->LinkedTo[0]->GetOwningNode())));
		}

		const int32 OldIndex = OldParentPin->LinkedTo.IndexOfByKey(NodeIn);
		if (OldIndex == INDEX_NONE)
		{
			// Unreachable while OldParentPin comes from LinkedTo[0], and checked anyway: the whole
			// bug this guards against was a break that quietly did nothing followed by a "restore"
			// that invented a link. Never relink on the strength of an index we do not have.
			return FString::Printf(
				TEXT("'%s' is not present in its parent's link order; refusing to move it rather than "
					 "risk relinking it somewhere it never was"),
				*NodePath);
		}

		// Unlinked first, because CanCreateConnection answers BREAK_OTHERS_* — not MAKE — for a
		// child that still has a parent, and this code refuses anything but MAKE. The window is
		// safe: no notification fires (see the file header), so nothing regenerates the runtime
		// tree while the node is detached, and a refusal below puts it back exactly where it was.
		OldParent->Modify();
		OldParentPin->BreakLinkTo(NodeIn);

		const FString LinkError = CheckLink(Graph, Slot.Pin, NodeIn, /*bAllowOccupied*/ false);
		if (!LinkError.IsEmpty())
		{
			OldParentPin->MakeLinkTo(NodeIn);
			PlaceLinkAt(OldParentPin, NodeIn, OldIndex);
			return LinkError;
		}

		Slot.Pin->MakeLinkTo(NodeIn);
		PlaceLinkAt(Slot.Pin, NodeIn, Slot.InsertPosition);
	}

	return VibeBT::CommitGraph(Tree, Graph);
}

namespace VibeBTEdit
{
	/** AddDecorator and AddService, which differ only in the base class, the slot and one gate. */
	FString AddSubNodeOfKind(const FString& AssetPath, const FString& NodePath,
		const FString& NodeClassName, int32 Index, ESubNodeKind Kind)
	{
		UBehaviorTree* Tree = nullptr;
		UBehaviorTreeGraph* Graph = nullptr;
		const FString GuardError = VibeBT::OpenWriteGuard(AssetPath, Tree, Graph);
		if (!GuardError.IsEmpty())
		{
			return FString::Printf(TEXT("ERROR: %s"), *GuardError);
		}

		UBehaviorTreeGraphNode* Owner = VibeBT::ResolveNodePath(Graph, NodePath);
		if (!Owner)
		{
			return FString::Printf(
				TEXT("ERROR: no node at path '%s' in %s (nothing there, or the path is ambiguous and "
					 "needs an index, as in 'Sequence[1]')"),
				*NodePath, *AssetPath);
		}

		// The engine's own gate, asked of the graph rather than of the node: it is what the Behavior
		// Tree editor's context menu consults before offering "Add Service"
		// (BehaviorTreeGraphNode_Composite.cpp:84, BehaviorTreeGraphNode_Task.cpp:52). Stock
		// UBehaviorTreeGraph answers true unconditionally (BehaviorTreeGraph.h:52) and no engine class
		// overrides it, so this only ever fires for a project that subclasses the graph to forbid
		// services. Honoured anyway: when a project does that, silently authoring services its own
		// editor refuses to show is precisely the unrequested write this service exists to avoid.
		if (Kind == ESubNodeKind::Service && !Graph->DoesSupportServices())
		{
			return FString::Printf(
				TEXT("ERROR: %s does not support services (%s::DoesSupportServices() is false)"),
				*AssetPath, *Graph->GetClass()->GetName());
		}

		if (Owner->bInjectedNode)
		{
			return FString::Printf(
				TEXT("ERROR: '%s' was injected from a subtree and is read-only here. The engine "
					 "regenerates injected nodes from the subtree asset, so anything attached to one "
					 "is discarded on the next graph update. Edit the subtree asset instead."),
				*NodePath);
		}

		// Checked before the class is resolved and before anything is created, so a refused add leaves
		// no stray node behind.
		const FString CarryError = CheckCanCarrySubNodes(Owner);
		if (!CarryError.IsEmpty())
		{
			return FString::Printf(TEXT("ERROR: %s"), *CarryError);
		}

		UClass* const RequiredBase = (Kind == ESubNodeKind::Decorator)
			? UBTDecorator::StaticClass()
			: UBTService::StaticClass();
		UClass* NodeClass = VibeBT::ResolveNodeClass(NodeClassName, RequiredBase);
		if (!NodeClass)
		{
			return FString::Printf(
				TEXT("ERROR: unknown or ambiguous Behavior Tree %s class '%s'. Use "
					 "GetAvailableNodeTypes(\"%s\") to list what is available."),
				SubNodeKindName(Kind), *NodeClassName,
				Kind == ESubNodeKind::Decorator ? TEXT("Decorator") : TEXT("Service"));
		}

		FString AttachError;
		UBehaviorTreeGraphNode* SubNode =
			AttachSubNode(Graph, Owner, NodeClass, Kind, Index, AttachError);
		if (!SubNode)
		{
			return FString::Printf(TEXT("ERROR: %s"), *AttachError);
		}

		const FString CommitError = VibeBT::CommitGraph(Tree, Graph);
		if (!CommitError.IsEmpty())
		{
			return FString::Printf(TEXT("ERROR: %s"), *CommitError);
		}

		return VibeBT::GetNodePath(SubNode);
	}
}

FString UBehaviorTreeService::AddDecorator(const FString& AssetPath, const FString& NodePath,
	const FString& DecoratorClassName, int32 Index)
{
	return AddSubNodeOfKind(AssetPath, NodePath, DecoratorClassName, Index, ESubNodeKind::Decorator);
}

FString UBehaviorTreeService::AddService(const FString& AssetPath, const FString& NodePath,
	const FString& ServiceClassName, int32 Index)
{
	return AddSubNodeOfKind(AssetPath, NodePath, ServiceClassName, Index, ESubNodeKind::Service);
}

FString UBehaviorTreeService::RemoveSubNode(const FString& AssetPath, const FString& SubNodePath)
{
	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	const FString GuardError = VibeBT::OpenWriteGuard(AssetPath, Tree, Graph);
	if (!GuardError.IsEmpty())
	{
		return GuardError;
	}

	UBehaviorTreeGraphNode* SubNode = VibeBT::ResolveNodePath(Graph, SubNodePath);
	if (!SubNode)
	{
		return FString::Printf(
			TEXT("No sub-node at path '%s' in %s. Decorators and services are addressed by index on "
				 "their owner, as in 'Root/Selector[0]/@decorator[0]'."),
			*SubNodePath, *AssetPath);
	}

	if (SubNode->bInjectedNode)
	{
		return FString::Printf(
			TEXT("'%s' was injected from a subtree and cannot be removed here: it is a copy that "
				 "UBehaviorTreeGraphNode_SubtreeTask::UpdateInjectedNodes rebuilds from the subtree "
				 "asset, so removing it would report success and come straight back. Remove it from the "
				 "subtree asset instead."),
			*SubNodePath);
	}

	UBehaviorTreeGraphNode* Owner = nullptr;
	ESubNodeKind Kind = ESubNodeKind::Decorator;
	int32 SlotIndex = INDEX_NONE;
	const FString SlotError = ResolveSubNodeSlot(SubNode, SubNodePath, Owner, Kind, SlotIndex);
	if (!SlotError.IsEmpty())
	{
		return SlotError;
	}

	// Data only, and deliberately not DestroyNode: a sub-node is not in UEdGraph::Nodes, and
	// UAIGraphNode::DestroyNode routes back through ParentNode->RemoveSubNode into
	// UEdGraphNode::DestroyNode. Dropping the two references is the whole removal — the graph node
	// becomes unreferenced, and its now-orphaned UBTDecorator/UBTService instance is renamed into the
	// transient package by the RemoveOrphanedNodes() that ends CreateBTFromGraph inside the commit.
	Graph->Modify();
	Owner->Modify();
	SlotArray(Owner, Kind).RemoveAt(SlotIndex);
	Owner->SubNodes.RemoveSingle(SubNode);
	SubNode->ParentNode = nullptr;

	return VibeBT::CommitGraph(Tree, Graph);
}

FString UBehaviorTreeService::SetNodeName(const FString& AssetPath, const FString& NodePath,
	const FString& NewName)
{
	// Validated before the asset is opened: the name becomes the node's path segment, and one the
	// grammar cannot express would leave the node un-addressable by any later call.
	const FString GrammarError = CheckNodeNameGrammar(NewName);
	if (!GrammarError.IsEmpty())
	{
		return GrammarError;
	}

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

	if (Node->bInjectedNode)
	{
		return FString::Printf(
			TEXT("'%s' was injected from a subtree and is read-only here; the engine rebuilds it from "
				 "the subtree asset, which is where its name belongs."),
			*NodePath);
	}

	// The name lives on the node INSTANCE, not on the graph node: UBTNode::NodeName is what
	// GetNodeName() returns and what every UBehaviorTreeGraphNode subclass reports as its title. The
	// graph's Root node carries no instance, so it has no name to set.
	UBTNode* Instance = Cast<UBTNode>(ToRawPtr(Node->NodeInstance));
	if (!Instance)
	{
		return FString::Printf(
			TEXT("'%s' carries no Behavior Tree node instance, so it has no name to set (the graph's "
				 "Root node is always in this state)"),
			*NodePath);
	}

	Instance->Modify();
	Instance->NodeName = NewName;

	return VibeBT::CommitGraph(Tree, Graph);
}

FString UBehaviorTreeService::SetNodeComment(const FString& AssetPath, const FString& NodePath,
	const FString& Comment)
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

	if (Node->bInjectedNode)
	{
		return FString::Printf(
			TEXT("'%s' was injected from a subtree and is read-only here; comment the node in the "
				 "subtree asset instead."),
			*NodePath);
	}

	// The comment lives on the GRAPH node (UEdGraphNode::NodeComment — the editor's comment
	// bubble), not on the node instance, so unlike SetNodeName it changes no path segment and no
	// runtime behaviour: it is pure annotation, and the natural place for an agent to record WHY a
	// subtree exists. An empty Comment clears it. The graph's Root node is allowed — a tree-level
	// comment is a legitimate thing to want.
	Node->Modify();
	Node->NodeComment = Comment;
	Node->bCommentBubbleVisible = !Comment.IsEmpty();

	return VibeBT::CommitGraph(Tree, Graph);
}

namespace VibeBTEdit
{
	/**
	 * The node instance at NodePath, ready to be written, or null with Result.Error set.
	 *
	 * The echo fields are filled BEFORE any refusal, so a caller whose path resolved to the wrong
	 * node can see that from the response rather than from the error text alone.
	 *
	 * The injected-node guard is here rather than left to the structural mutators' natural barriers:
	 * those bail on a null input pin or in ResolveChildSlot, and a property write has no such
	 * barrier — it resolves a path and writes. An injected node is a copy that
	 * UBehaviorTreeGraphNode_SubtreeTask::UpdateInjectedNodes rebuilds from the subtree asset, so a
	 * property written onto one reports success, saves, and is silently replaced by the subtree's
	 * value on the next graph update.
	 */
	UObject* ResolveInstanceForWrite(UBehaviorTreeGraph* Graph, const FString& NodePath,
		FBTPropertySetResult& Result)
	{
		UBehaviorTreeGraphNode* Node = VibeBT::ResolveNodePath(Graph, NodePath);
		if (!Node)
		{
			Result.Error = FString::Printf(
				TEXT("No node at path '%s' (nothing there, or the path is ambiguous and needs an index, "
					 "as in 'Sequence[1]')"),
				*NodePath);
			return nullptr;
		}

		Result.ResolvedNodeClass = Node->NodeInstance
			? Node->NodeInstance->GetClass()->GetName()
			: FString();
		Result.ResolvedNodeName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();

		if (Node->bInjectedNode)
		{
			Result.Error = FString::Printf(
				TEXT("'%s' was injected from a subtree and is read-only here. The engine regenerates "
					 "injected nodes from the subtree asset, so a property written on one is discarded "
					 "on the next graph update. Edit the subtree asset instead."),
				*NodePath);
			return nullptr;
		}

		UObject* Instance = ToRawPtr(Node->NodeInstance);
		if (!Instance)
		{
			Result.Error = FString::Printf(
				TEXT("'%s' carries no Behavior Tree node instance, so it has no properties to set (the "
					 "graph's Root node is always in this state)"),
				*NodePath);
			return nullptr;
		}
		return Instance;
	}

	/**
	 * Whether the selector's type filter admits this key — the same test
	 * FBlackboardKeySelector::InitSelection applies when it picks a key for itself
	 * (BehaviorTreeTypes.cpp:571-586), and the same one FBlackboardKeySelector::PreSave applies
	 * when it decides whether to keep a binding at save time (:684-706).
	 *
	 * ResolveSelectedKey, notably, does NOT apply it: a mistyped key resolves to a perfectly valid
	 * ID, so the ID check alone would let this through.
	 */
	bool IsKeyAllowedBySelector(const FBlackboardKeySelector& Selector, const FBlackboardEntry& Entry)
	{
		if (Selector.AllowedTypes.Num() == 0)
		{
			return true;
		}
		if (!Entry.KeyType)
		{
			return false;
		}
		for (const TObjectPtr<UBlackboardKeyType>& Filter : Selector.AllowedTypes)
		{
			if (Entry.KeyType->IsAllowedByFilter(ToRawPtr(Filter)))
			{
				return true;
			}
		}
		return false;
	}

	/** "Object, Vector" — the filter classes a selector accepts, for an error message. */
	FString DescribeAllowedTypes(const FBlackboardKeySelector& Selector)
	{
		TArray<FString> Names;
		for (const TObjectPtr<UBlackboardKeyType>& Filter : Selector.AllowedTypes)
		{
			if (const UBlackboardKeyType* Raw = ToRawPtr(Filter))
			{
				FString Name = Raw->GetClass()->GetName();
				Name.RemoveFromStart(TEXT("BlackboardKeyType_"));
				Names.AddUnique(Name);
			}
		}
		return Names.Num() > 0 ? FString::Join(Names, TEXT(", ")) : FString(TEXT("<any>"));
	}
}

FBTPropertySetResult UBehaviorTreeService::SetNodePropertyValue(const FString& AssetPath,
	const FString& NodePath, const FString& PropertyName, const FString& Value)
{
	FBTPropertySetResult Result;

	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	Result.Error = VibeBT::OpenWriteGuard(AssetPath, Tree, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}

	UObject* Instance = ResolveInstanceForWrite(Graph, NodePath, Result);
	if (!Instance)
	{
		return Result;
	}

	FProperty* Property = VibeBT::FindAuthorableProperty(Instance, PropertyName, Result.Error);
	if (!Property)
	{
		return Result;
	}

	// UBTNode::NodeName through reflection IS SetNodeName, so the same path-grammar refusals apply:
	// without this, "NodeName" = "a/b" renames the node into un-addressability through the one
	// route SetNodeName's own validation never sees — including BuildTree's replay of a dump whose
	// name a human typed into the details panel, which has no such check of its own.
	if (PropertyName == TEXT("NodeName"))
	{
		Result.Error = CheckNodeNameGrammar(Value);
		if (!Result.Error.IsEmpty())
		{
			return Result;
		}
	}

	// The pre-image, so a partial import can be undone: ImportText applies a struct or array literal
	// member by member and returns null where it fails, leaving everything it had already applied in
	// place — a refused write that half-changed the node would be worse than either outcome.
	//
	// Copied at the VALUE level, not exported to text. A text pre-image cannot restore a struct:
	// UScriptStruct::ExportText omits members sitting at the struct's default, so re-importing
	// "(DefaultValue=3.500000)" leaves behind whatever member the failed import had already written.
	// That is not hypothetical — it is what the first version of this rollback did, and a refused
	// write survived on the node as Key="Hijacked" while the caller was told nothing had changed.
	void* const Snapshot = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
	Property->InitializeValue(Snapshot);
	Property->CopyCompleteValue(Snapshot, Property->ContainerPtrToValuePtr<void>(Instance));
	ON_SCOPE_EXIT
	{
		Property->DestroyValue(Snapshot);
		FMemory::Free(Snapshot);
	};

	Instance->Modify();

	// Errors captured rather than sent to GWarn: they belong in the response, and a tool that logs
	// an error and returns a success-shaped result is exactly what this service exists to avoid.
	FStringOutputDevice ImportErrors;
	const TCHAR* Imported =
		Property->ImportText_InContainer(*Value, Instance, Instance, PPF_None, &ImportErrors);
	if (Imported == nullptr)
	{
		Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Instance), Snapshot);

		Result.Error = FString::Printf(TEXT("could not parse '%s' as %s (%s::%s)"),
			*Value, *Property->GetCPPType(), *Instance->GetClass()->GetName(), *PropertyName);
		if (!ImportErrors.IsEmpty())
		{
			Result.Error += TEXT(": ") + FString(ImportErrors).TrimStartAndEnd();
		}
		VibeBT::ExportPropertyValue(Property, Instance, Result.ValueAfterWrite);
		return Result;
	}

	// What the import actually produced, captured before the commit can rewrite it.
	FString Intended;
	VibeBT::ExportPropertyValue(Property, Instance, Intended);

	Result.Error = VibeBT::CommitGraph(Tree, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}

	// Read back AFTER the commit, never an echo of the input, and then checked against what was
	// written — because the commit is not a passive save. UBTNode::PreSave walks every top-level
	// struct property of every node and rewrites two whole families of them (BTNode.cpp:231-254):
	// FBlackboardKeySelector, and FValueOrBlackboardKeyBase — which in UE 5.8 is what an ordinary
	// numeric property like BTTask_Wait::WaitTime now is. Both reset a binding whose key is missing
	// or of the wrong type, and both do it at save time, after this code has already succeeded. A
	// write that did not survive that is a failed write, not a successful one with a surprising
	// read-back.
	VibeBT::ExportPropertyValue(Property, Instance, Result.ValueAfterWrite);
	if (Result.ValueAfterWrite != Intended)
	{
		Result.Error = FString::Printf(
			TEXT("%s::%s was written as '%s' but the asset saved '%s'. The commit rewrote it — for a "
				 "blackboard binding this means the key does not exist on %s, or its type is not one "
				 "the property accepts, and UBTNode::PreSave cleared it. The asset on disk holds the "
				 "value reported here, not the one that was asked for."),
			*Instance->GetClass()->GetName(), *PropertyName, *Intended, *Result.ValueAfterWrite,
			*GetNameSafe(ToRawPtr(Tree->BlackboardAsset)));
		return Result;
	}

	Result.bSuccess = true;
	return Result;
}

FBTPropertySetResult UBehaviorTreeService::SetNodeBlackboardKey(const FString& AssetPath,
	const FString& NodePath, const FString& PropertyName, const FString& KeyName)
{
	FBTPropertySetResult Result;

	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	Result.Error = VibeBT::OpenWriteGuard(AssetPath, Tree, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}

	UObject* Instance = ResolveInstanceForWrite(Graph, NodePath, Result);
	if (!Instance)
	{
		return Result;
	}

	FProperty* Property = VibeBT::FindAuthorableProperty(Instance, PropertyName, Result.Error);
	if (!Property)
	{
		return Result;
	}

	FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	if (!StructProperty || StructProperty->Struct != FBlackboardKeySelector::StaticStruct())
	{
		Result.Error = FString::Printf(
			TEXT("%s::%s is not a blackboard key selector (it is %s). Use SetNodePropertyValue for "
				 "ordinary properties."),
			*Instance->GetClass()->GetName(), *PropertyName, *Property->GetCPPType());
		return Result;
	}

	UBlackboardData* Board = ToRawPtr(Tree->BlackboardAsset);
	if (!Board)
	{
		Result.Error = FString::Printf(
			TEXT("%s has no blackboard asset, so there is nothing to resolve '%s' against. Assign one "
				 "with SetBlackboardAsset first."),
			*AssetPath, *KeyName);
		return Result;
	}

	// Validated against the board BEFORE the selector is touched, so a refused binding leaves the
	// node exactly as it was — and so the engine's own "key not found" warning never fires.
	const FName Key(*KeyName);
	const FBlackboard::FKey KeyID = Board->GetKeyID(Key);
	const FBlackboardEntry* Entry = (KeyID != FBlackboard::InvalidKey) ? Board->GetKey(KeyID) : nullptr;
	if (!Entry)
	{
		Result.Error = FString::Printf(
			TEXT("blackboard %s has no key '%s'. Use GetBlackboardKeys to list what is there."),
			*Board->GetName(), *KeyName);
		return Result;
	}

	FBlackboardKeySelector* Selector =
		StructProperty->ContainerPtrToValuePtr<FBlackboardKeySelector>(Instance);
	if (!IsKeyAllowedBySelector(*Selector, *Entry))
	{
		FString KeyTypeName = Entry->KeyType ? Entry->KeyType->GetClass()->GetName() : FString(TEXT("<none>"));
		KeyTypeName.RemoveFromStart(TEXT("BlackboardKeyType_"));
		Result.Error = FString::Printf(
			TEXT("key '%s' on %s is of type %s, which %s::%s does not accept (it accepts: %s). Binding "
				 "it anyway would resolve to a valid key ID and still never work, and on the next save "
				 "UBTNode::PreSave would silently replace it — with None, or, if the selector allows "
				 "None as a value, with whatever key InitSelection happens to match first."),
			*KeyName, *Board->GetName(), *KeyTypeName, *Instance->GetClass()->GetName(), *PropertyName,
			*DescribeAllowedTypes(*Selector));
		return Result;
	}

	const FName PreviousKey = Selector->SelectedKeyName;
	Instance->Modify();
	Selector->SelectedKeyName = Key;
	Selector->ResolveSelectedKey(*Board);

	// Backstop. Unreachable given the two checks above — both of the engine's own failure conditions
	// have already been ruled out — and kept because an unresolved ID is a silent runtime no-op, and
	// this is the only place that can still see one.
	if (Selector->GetSelectedKeyID() == FBlackboard::InvalidKey)
	{
		Selector->SelectedKeyName = PreviousKey;
		Selector->InvalidateResolvedKey();
		if (!PreviousKey.IsNone())
		{
			Selector->ResolveSelectedKey(*Board);
		}
		Result.Error = FString::Printf(
			TEXT("key '%s' exists on %s and is of an accepted type, yet the selector would not resolve "
				 "it; refusing rather than saving a node that silently does nothing"),
			*KeyName, *Board->GetName());
		return Result;
	}

	Result.Error = VibeBT::CommitGraph(Tree, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}

	// Verified after the commit, because the commit is where a binding is undone: UpdateAsset
	// re-resolves every selector against the tree's board, and the package save runs
	// UBTNode::PreSave over all of them.
	VibeBT::ExportPropertyValue(Property, Instance, Result.ValueAfterWrite);
	if (Selector->SelectedKeyName != Key || Selector->GetSelectedKeyID() == FBlackboard::InvalidKey)
	{
		Result.Error = FString::Printf(
			TEXT("the commit did not keep the binding of %s::%s to '%s' (it is now '%s'); the saved "
				 "asset does not have the requested binding"),
			*Instance->GetClass()->GetName(), *PropertyName, *KeyName,
			*Selector->SelectedKeyName.ToString());
		return Result;
	}

	Result.bSuccess = true;
	return Result;
}
