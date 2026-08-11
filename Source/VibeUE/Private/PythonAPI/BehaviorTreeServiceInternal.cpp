// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "BehaviorTreeServiceInternal.h"

#include "AIGraphTypes.h"
#include "Algo/Reverse.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace VibeBT
{
	namespace
	{
		/** Number of leaf columns the subtree occupies. Leaves occupy exactly one. */
		int32 CountLeafColumns(const FLayoutNode& Node)
		{
			if (Node.Children.Num() == 0)
			{
				return 1;
			}
			int32 Total = 0;
			for (const FLayoutNode& Child : Node.Children)
			{
				Total += CountLeafColumns(Child);
			}
			return Total;
		}

		/**
		 * Pre-order assignment. NextColumn is the leftmost free leaf column; a parent is
		 * centred over the columns its subtree consumes.
		 */
		void AssignPositions(const FLayoutNode& Node, int32 Depth, int32& NextColumn,
			TArray<FIntPoint>& Out)
		{
			const int32 SelfIndex = Out.AddDefaulted();
			const int32 FirstColumn = NextColumn;

			if (Node.Children.Num() == 0)
			{
				++NextColumn;
			}
			else
			{
				for (const FLayoutNode& Child : Node.Children)
				{
					AssignPositions(Child, Depth + 1, NextColumn, Out);
				}
			}

			const int32 LastColumn = NextColumn - 1;
			// Centre over the span: (a + b) * 300 is always even, so dividing by 2 always
			// lands on an integer, which is necessary to preserve strict sibling ordering.
			Out[SelfIndex].X = (FirstColumn + LastColumn) * NodeSpacingX / 2;
			Out[SelfIndex].Y = Depth * NodeSpacingY;
		}

		/** GetChildNodes, minus anything already visited — the cycle guard the layout walk needs. */
		void GatherChildren(UBehaviorTreeGraphNode* Node, TArray<UBehaviorTreeGraphNode*>& Out,
			const TSet<UBehaviorTreeGraphNode*>& Visited)
		{
			Out = GetChildNodes(Node);
			Out.RemoveAll([&Visited](UBehaviorTreeGraphNode* Child)
				{ return Visited.Contains(Child); });
		}

		/** Build the structure mirror and, in the same pre-order, the node list to write back. */
		void BuildMirror(UBehaviorTreeGraphNode* Node, FLayoutNode& OutNode,
			TArray<UBehaviorTreeGraphNode*>& OutOrder, TSet<UBehaviorTreeGraphNode*>& Visited)
		{
			if (!Node || Visited.Contains(Node))
			{
				return;
			}

			Visited.Add(Node);
			OutOrder.Add(Node);

			TArray<UBehaviorTreeGraphNode*> Children;
			GatherChildren(Node, Children, Visited);

			// Deduplicate: if the same child appears twice (duplicate pins), keep only the first.
			// This must happen before allocating Mirror slots, so allocation matches recursion.
			TSet<UBehaviorTreeGraphNode*> UniqueChildren(Children);
			TArray<UBehaviorTreeGraphNode*> FilteredChildren;
			FilteredChildren.Reserve(UniqueChildren.Num());
			for (UBehaviorTreeGraphNode* Child : Children)
			{
				if (UniqueChildren.Contains(Child))
				{
					FilteredChildren.Add(Child);
					UniqueChildren.Remove(Child);
				}
			}

			OutNode.Children.AddDefaulted(FilteredChildren.Num());
			for (int32 Index = 0; Index < FilteredChildren.Num(); ++Index)
			{
				BuildMirror(FilteredChildren[Index], OutNode.Children[Index], OutOrder, Visited);
			}
		}
	}

	TArray<FIntPoint> ComputeLayout(const FLayoutNode& Root)
	{
		TArray<FIntPoint> Positions;

		int32 NextColumn = 0;
		AssignPositions(Root, 0, NextColumn, Positions);
		return Positions;
	}

	void ArrangeGraph(UBehaviorTreeGraphNode* RootNode)
	{
		if (!RootNode)
		{
			return;
		}

		FLayoutNode Mirror;
		TArray<UBehaviorTreeGraphNode*> Order;
		TSet<UBehaviorTreeGraphNode*> Visited;
		BuildMirror(RootNode, Mirror, Order, Visited);

		const TArray<FIntPoint> Positions = ComputeLayout(Mirror);
		check(Positions.Num() == Order.Num());

		for (int32 Index = 0; Index < Order.Num(); ++Index)
		{
			Order[Index]->Modify();
			Order[Index]->NodePosX = Positions[Index].X;
			Order[Index]->NodePosY = Positions[Index].Y;
		}
	}

	UBehaviorTreeGraphNode_Root* FindRootGraphNode(UBehaviorTreeGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UBehaviorTreeGraphNode_Root* Root = Cast<UBehaviorTreeGraphNode_Root>(Node))
			{
				return Root;
			}
		}
		return nullptr;
	}

	namespace
	{
		/** Whether anything at all hangs off the graph root's output pin. */
		bool GraphRootHasLink(const UBehaviorTreeGraphNode_Root* Root)
		{
			return Root && Root->Pins.Num() > 0 && Root->Pins[0] && Root->Pins[0]->LinkedTo.Num() > 0;
		}

		/**
		 * Whether committing this graph would leave the asset with a runtime tree, i.e. whether
		 * UBehaviorTree::RootNode would come out non-null on the other side of UpdateAsset().
		 *
		 * Deliberately mirrors the engine statement for statement, so it can never disagree with it
		 * about what is about to happen — UpdateAsset() picks the node:
		 *
		 *   if (RootNode && RootNode->Pins.Num() > 0 && RootNode->Pins[0]->LinkedTo.Num() > 0)
		 *       Node = Cast<UBehaviorTreeGraphNode>(RootNode->Pins[0]->LinkedTo[0]->GetOwningNode());
		 *   CreateBTFromGraph(Node);
		 *
		 * and CreateBTFromGraph turns it into the new tree:
		 *
		 *   BTAsset->RootNode = Cast<UBTCompositeNode>(RootEdNode->NodeInstance);
		 *
		 * That last cast is why the NodeInstance check below is not belt-and-braces. A root linked
		 * to a graph node carrying no instance (or a non-composite one) leaves BTAsset->RootNode
		 * null, and the very next line — BTGraphHelpers::CreateChildren(BTAsset, BTAsset->RootNode,
		 * ...) — dereferences it as RootNode->Children.Reset() behind a guard that only tests
		 * RootEdNode (BehaviorTreeGraph.cpp:510-517). So that shape does not merely discard the
		 * tree: it crashes the editor inside OnSave(). Refusing it here is the only place it can be
		 * stopped, because the post-OnSave check never gets to run.
		 */
		bool GraphWouldRebuildARuntimeTree(const UBehaviorTreeGraphNode_Root* Root)
		{
			if (!GraphRootHasLink(Root))
			{
				return false;
			}
			const UEdGraphPin* LinkedPin = Root->Pins[0]->LinkedTo[0];
			const UBehaviorTreeGraphNode* LinkedNode =
				LinkedPin ? Cast<UBehaviorTreeGraphNode>(LinkedPin->GetOwningNode()) : nullptr;
			return LinkedNode && Cast<UBTCompositeNode>(LinkedNode->NodeInstance) != nullptr;
		}
	}

	FString EnsureGraph(UBehaviorTree* Tree)
	{
		if (!Tree)
		{
			return TEXT("EnsureGraph: null Behavior Tree");
		}

		if (!Tree->BTGraph)
		{
			// UBehaviorTreeFactory does not create the graph; FBehaviorTreeEditor does, lazily, when
			// the asset is first opened (BehaviorTreeEditor.cpp:345-352). Replicate that here so an
			// asset created headlessly is immediately writable.
			const TSubclassOf<UEdGraphSchema> SchemaClass = GetDefault<UBehaviorTreeGraph>()->Schema;
			if (!SchemaClass)
			{
				return TEXT("UBehaviorTreeGraph has no default schema class");
			}

			Tree->Modify();
			Tree->BTGraph = FBlueprintEditorUtils::CreateNewGraph(
				Tree, TEXT("Behavior Tree"), UBehaviorTreeGraph::StaticClass(), SchemaClass);
			if (!Tree->BTGraph)
			{
				return TEXT("failed to create BTGraph");
			}
		}

		UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
		if (!Graph)
		{
			return FString::Printf(TEXT("BTGraph is a %s, not a UBehaviorTreeGraph"),
				*Tree->BTGraph->GetClass()->GetName());
		}

		if (!FindRootGraphNode(Graph))
		{
			const UEdGraphSchema* Schema = Graph->GetSchema();
			if (!Schema)
			{
				return TEXT("Behavior Tree graph has no schema");
			}

			// Spawning the root must not change which blackboard the tree points at.
			// UBehaviorTreeGraphNode_Root::PostPlacedNewNode() — which FGraphNodeCreator::Finalize()
			// always runs — assigns the AI config's DefaultBlackboard or, failing that, whichever
			// UBlackboardData happens to be loaded first in this process, and pushes it onto the
			// owning asset. In the editor that is a convenience for a human who is about to pick one;
			// here it would silently bind a tree to an unrelated blackboard (and, for a tree that
			// deliberately has none, invent one), so the pre-existing value is restored below.
			UBlackboardData* const OriginalBlackboard = ToRawPtr(Tree->BlackboardAsset);

			// CreateDefaultNodesForGraph is what actually spawns the UBehaviorTreeGraphNode_Root
			// (EdGraphSchema_BehaviorTree.cpp:77). OnCreated() -> SpawnMissingNodes() does NOT: it
			// looks for an already-present root and rebuilds graph nodes underneath it from the
			// *runtime* tree, so on a factory-fresh asset it finds nothing and creates nothing. The
			// editor runs both, in this order (BehaviorTreeEditor.cpp:349-358).
			Graph->Modify();
			Schema->CreateDefaultNodesForGraph(*Graph);
			Graph->OnCreated();

			UBehaviorTreeGraphNode_Root* Root = FindRootGraphNode(Graph);
			if (!Root)
			{
				return TEXT("Behavior Tree graph root node was not created");
			}

			// Restored through the engine's own path rather than by writing the two pointers back.
			// The hijack above was not just an assignment: PostPlacedNewNode calls UpdateBlackboard(),
			// which runs UBehaviorTreeGraph::UpdateBlackboardChange() and re-runs InitializeFromAsset
			// on every node instance, decorator and service in the graph — re-resolving their cached
			// blackboard key IDs against the wrong board (BehaviorTreeGraph.cpp:50-95, and
			// FBlackboardKeySelector::ResolveSelectedKey). Assigning the pointers back would leave
			// those instances resolved against a board the asset no longer references.
			//
			// UpdateBlackboard() early-outs when the asset already points at this board, so this is
			// free when nothing was hijacked; otherwise it reassigns AND re-resolves. It is correct
			// for a null OriginalBlackboard too: InitializeFromAsset invalidates the resolved keys,
			// which is exactly right for a tree with no board.
			Root->BlackboardAsset = OriginalBlackboard;
			Root->UpdateBlackboard();
		}

		return FString();
	}

	FString OpenWriteGuard(const FString& AssetPath, UBehaviorTree*& OutTree,
		UBehaviorTreeGraph*& OutGraph)
	{
		OutTree = nullptr;
		OutGraph = nullptr;

		if (AssetPath.IsEmpty())
		{
			return TEXT("AssetPath is empty");
		}

		// LOAD_NoWarn | LOAD_Quiet: "not found" is a value this function returns to its caller, not
		// an incident worth engine warnings in the log.
		UBehaviorTree* Tree =
			LoadObject<UBehaviorTree>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Tree)
		{
			return FString::Printf(TEXT("Behavior Tree not found: %s"), *AssetPath);
		}

		// Both refusals are checked before EnsureGraph, so a refused write leaves the asset exactly
		// as it was. That is not cosmetic for the lock check: EnsureGraph on a graph that has lost
		// its root spawns one, Modify()s the graph and churns the blackboard binding of every node
		// instance in it (see the restore in EnsureGraph) — all of it on an asset we are about to
		// refuse to write.
		if (GEditor)
		{
			if (UAssetEditorSubsystem* AssetEditorSubsystem =
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				if (AssetEditorSubsystem->FindEditorsForAsset(Tree).Num() > 0)
				{
					return FString::Printf(
						TEXT("A Behavior Tree editor is open on %s; close it and retry. The open editor "
							 "holds its own copy of the graph, would not show this change, and would "
							 "overwrite it on the next save from the editor."),
						*AssetPath);
				}
			}
		}

		// Only an existing graph can be locked: EnsureGraph's freshly created one has bLockUpdates
		// clear by construction, and nothing between here and the return can set it.
		if (const UBehaviorTreeGraph* ExistingGraph = Cast<UBehaviorTreeGraph>(Tree->BTGraph))
		{
			if (ExistingGraph->IsLocked())
			{
				return FString::Printf(
					TEXT("Graph updates are locked on %s (bLockUpdates). UBehaviorTreeGraph::UpdateAsset() "
						 "early-returns while it is set, so the write would be saved with a stale runtime "
						 "tree and still report success."),
					*AssetPath);
			}
		}

		const FString GraphError = EnsureGraph(Tree);
		if (!GraphError.IsEmpty())
		{
			return GraphError;
		}

		UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
		if (!Graph)
		{
			return FString::Printf(TEXT("%s has no Behavior Tree graph"), *AssetPath);
		}

		OutTree = Tree;
		OutGraph = Graph;
		return FString();
	}

	FString CommitGraph(UBehaviorTree* Tree, UBehaviorTreeGraph* Graph)
	{
		if (!Tree || !Graph)
		{
			return TEXT("CommitGraph: null Behavior Tree or graph");
		}

		// Re-asserted rather than assumed: OpenWriteGuard checked this, but anything the caller did
		// in between could have locked the graph, and a locked UpdateAsset() below does nothing at
		// all — silently, so an entire batch would be saved with a stale runtime tree.
		if (Graph->IsLocked())
		{
			return TEXT("Graph updates are locked (bLockUpdates); UBehaviorTreeGraph::UpdateAsset() "
						"would silently do nothing");
		}

		UBehaviorTreeGraphNode_Root* Root = FindRootGraphNode(Graph);
		if (!Root)
		{
			return TEXT("Behavior Tree graph has no root node");
		}

		// Refuse to trade a populated runtime tree for an empty graph.
		//
		// UpdateAsset() -> CreateBTFromGraph() opens by discarding the runtime tree outright
		// ("//discard old tree": RootNode = nullptr, RootDecorators/RootDecoratorOps emptied) and
		// rebuilds it from the graph — and it only has something to rebuild from when the root
		// node's output pin leads somewhere (see GraphRootFeedsANode). Committing a graph whose
		// root feeds nothing over an asset whose node tree is intact therefore replaces that tree
		// with an empty one, on disk, with no undo.
		//
		// That combination — populated runtime tree, graph root feeding nothing — is precisely a
		// graph-less or sparse-graph asset whose reconstruction either never ran or produced
		// nothing. EnsureGraph triggers that reconstruction (OnCreated -> SpawnMissingNodes, which
		// walks RootNode/Children/Decorators/Services and links the pins), so on a healthy asset
		// the graph is already repopulated by the time anything reaches here and this never fires.
		// When it does fire, the reconstruction failed and the right answer is to stop, not to
		// save the result.
		if (Tree->RootNode && !GraphWouldRebuildARuntimeTree(Root))
		{
			return FString::Printf(
				TEXT("Refusing to commit %s: its editor graph would not rebuild a runtime tree (the "
					 "root node leads to nothing, or to a node carrying no composite instance) while "
					 "the asset still has a populated runtime node tree. Saving would discard that "
					 "tree — UBehaviorTreeGraph::CreateBTFromGraph rebuilds it from the graph, and an "
					 "empty graph rebuilds an empty tree. The asset on disk is unchanged. Run "
					 "RepairGraphFromRuntimeTree to rebuild the graph from the runtime tree first."),
				*Tree->GetPathName());
		}

		// Never UBehaviorTreeGraph::AutoArrange(): it dereferences RootNode->DEPRECATED_NodeWidget
		// (BehaviorTreeGraph.cpp:1302), the Slate widget, which is only ever set while a Behavior
		// Tree editor tab is open — it crashes the editor outright when called headlessly.
		ArrangeGraph(Root);

		// OnSave() is SpawnMissingNodesForParallel() + UpdateAsset(): exactly what the BT editor runs
		// on save, and what regenerates UBehaviorTree::RootNode from the graph. A bare UpdateAsset()
		// would skip the parallel-task fix-up.
		const bool bHadRuntimeTree = Tree->RootNode != nullptr;
		Graph->OnSave();

		// Second line of defence, bracketing the rebuild rather than predicting it. The check above
		// models UpdateAsset's own root-selection test exactly, but it cannot model everything that
		// happens after: CreateBTFromGraph assigns Cast<UBTCompositeNode>(RootEdNode->NodeInstance),
		// which is still null when the root leads to a node carrying no instance, or one that is not
		// a composite. This is the last point at which the file can still be protected — the
		// in-memory tree is already gone, so the only safe thing left is to not persist it.
		if (bHadRuntimeTree && !Tree->RootNode)
		{
			return FString::Printf(
				TEXT("Refusing to save %s: rebuilding the runtime tree from the graph produced no root "
					 "node, which would have replaced a populated tree with an empty one. Nothing was "
					 "written — the asset on disk is unchanged — but this in-memory copy is now stale "
					 "and must be reloaded before it is used again."),
				*Tree->GetPathName());
		}

		UPackage* Package = Tree->GetOutermost();
		if (!Package)
		{
			return TEXT("Behavior Tree has no package");
		}

		// SavePackages(..., bOnlyDirty = true) silently skips a clean package, and neither OnSave()
		// nor a node-position-only change necessarily dirties one — hence both the explicit
		// Modify/MarkPackageDirty and bOnlyDirty = false.
		Tree->Modify();
		Tree->MarkPackageDirty();
		if (!UEditorLoadingAndSavingUtils::SavePackages({ Package }, /*bOnlyDirty*/ false))
		{
			return FString::Printf(TEXT("Failed to save package %s"), *Package->GetName());
		}

		return FString();
	}

	namespace
	{
		/** One FGraphNodeClassHelper per base class. GatherClasses/GetClass() are expensive
		 *  (the latter loads the class), so a single primed helper is reused for the module's
		 *  lifetime rather than rebuilt on every discovery call. */
		TMap<UClass*, TSharedPtr<FGraphNodeClassHelper>> GClassHelperCache;
	}

	UClass* ResolveNodeClass(const FString& ClassName, UClass* RequiredBase)
	{
		if (ClassName.IsEmpty() || !RequiredBase)
		{
			return nullptr;
		}

		// Fast path: an exact, already-loaded full object path (e.g. "/Script/AIModule.BTTask_MoveTo"
		// or "/Game/AI/BTT_Foo.BTT_Foo_C"). A full object path is unique by construction — unlike
		// FindFirstObject's unscoped short-name search below that this deliberately does NOT use,
		// there is no cross-package ambiguity to resolve here. This only ever short-circuits work;
		// when it misses (not yet loaded), we fall through to the helper-based route below, which
		// is the only route that can actually load a Blueprint class from disk.
		if (UClass* Loaded = FindObject<UClass>(nullptr, *ClassName))
		{
			return Loaded->IsChildOf(RequiredBase) ? Loaded : nullptr;
		}

		// Everything else — short native name ("BTTask_MoveTo"), Blueprint generated-class name
		// with or without "_C", and a not-yet-loaded Blueprint's full path — is matched against
		// the primed, RequiredBase-scoped class list rather than a global object-name search.
		// This is what makes an unloaded Blueprint class resolvable at all (only
		// FGraphNodeClassData::GetClass() loads from disk; FindObject/FindFirstObject never do),
		// and it keeps matches scoped to RequiredBase's own hierarchy, so a same-named class
		// under a different node category can never be the accidental winner.
		const TSharedPtr<FGraphNodeClassHelper> Helper = GetClassHelper(RequiredBase);
		if (!Helper.IsValid())
		{
			return nullptr;
		}

		TArray<FGraphNodeClassData> ClassData;
		Helper->GatherClasses(RequiredBase, ClassData);

		const FString GeneratedName =
			ClassName.EndsWith(TEXT("_C")) ? ClassName : ClassName + TEXT("_C");

		FGraphNodeClassData* Match = nullptr;
		int32 MatchCount = 0;
		for (FGraphNodeClassData& Data : ClassData)
		{
			const FString CandidateName = Data.GetClassName();
			bool bNameMatches = (CandidateName == ClassName);
			if (!bNameMatches && Data.IsBlueprint())
			{
				bNameMatches = (CandidateName == GeneratedName) ||
					(Data.GetPackageName() + TEXT(".") + CandidateName == ClassName);
			}

			if (bNameMatches)
			{
				++MatchCount;
				Match = &Data;
			}
		}

		if (MatchCount != 1)
		{
			// Zero: unresolved. More than one: two classes under RequiredBase share this short
			// name in different packages — silently picking one would be exactly the class of
			// bug documented in docs/gotchas.md (display-name / short-name collisions resolving
			// to the wrong class with no error). Refuse instead of guessing.
			return nullptr;
		}

		// Only now, on the single surviving candidate, does GetClass() run — LoadPackage +
		// FullyLoad for a Blueprint entry. Every rejected candidate above was matched by name
		// alone, so this never loads more than the one class actually being resolved.
		UClass* Resolved = Match->GetClass(/*bSilent=*/true);
		return (Resolved && Resolved->IsChildOf(RequiredBase)) ? Resolved : nullptr;
	}

	TSharedPtr<FGraphNodeClassHelper> GetClassHelper(UClass* BaseClass)
	{
		if (!BaseClass)
		{
			return nullptr;
		}

		if (const TSharedPtr<FGraphNodeClassHelper>* Existing = GClassHelperCache.Find(BaseClass))
		{
			return *Existing;
		}

		TSharedPtr<FGraphNodeClassHelper> Helper = MakeShared<FGraphNodeClassHelper>(BaseClass);

		// Without this priming step, GatherClasses() only ever reports native classes:
		// Blueprint-derived BT nodes (most of this project's tasks/decorators/services) are
		// silently invisible.
		FGraphNodeClassHelper::AddObservedBlueprintClasses(BaseClass);
		Helper->UpdateAvailableBlueprintClasses();

		GClassHelperCache.Add(BaseClass, Helper);
		return Helper;
	}

	bool IsAuthorableProperty(const FProperty* Property)
	{
		return Property
			&& Property->HasAnyPropertyFlags(CPF_Edit)
			&& !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient | CPF_Deprecated);
	}

	void ExportPropertyValue(const FProperty* Property, const UObject* Instance, FString& OutValue)
	{
		OutValue.Reset();
		if (!Property || !Instance)
		{
			return;
		}

		// Instance is deliberately passed as its own DELTA container. That is not a diff against
		// itself: ExportText_InContainer resolves the delta pointer to the same address as the value
		// (ContainerPtrToValuePtrForDefaults(NULL, ...) -> IsInContainer(nullptr) -> MAX_int32 ->
		// true), and FProperty::ExportText_Direct opens with `if (Data==Delta || ...)` -> export.
		// The delta is then handed down unchanged, so every nested member takes the same
		// short-circuit. See the header for what that buys.
		Property->ExportText_InContainer(0, OutValue, Instance, /*Delta*/ Instance,
			const_cast<UObject*>(Instance), PPF_None);
	}

	FProperty* FindAuthorableProperty(const UObject* Instance, const FString& PropertyName,
		FString& OutError)
	{
		if (!Instance)
		{
			OutError = TEXT("node carries no instance, so it has no properties");
			return nullptr;
		}

		UClass* Class = Instance->GetClass();
		FProperty* Property = Class->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			OutError = FString::Printf(
				TEXT("%s has no property '%s'. Use GetNodePropertyNames to list what is settable."),
				*Class->GetName(), *PropertyName);
			return nullptr;
		}

		if (!IsAuthorableProperty(Property))
		{
			// Deliberately distinct from "no such property": the name is right and the answer is
			// still no, which is a different thing for a caller to know.
			OutError = FString::Printf(
				TEXT("%s::%s is not editable (it is structure, transient or derived state that the "
					 "commit regenerates), so setting it would report success and change nothing"),
				*Class->GetName(), *PropertyName);
			return nullptr;
		}

		return Property;
	}

	TArray<UBehaviorTreeGraphNode*> GetChildNodes(const UBehaviorTreeGraphNode* Node)
	{
		TArray<UBehaviorTreeGraphNode*> Children;
		if (!Node)
		{
			return Children;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (!Linked)
				{
					continue;
				}
				if (UBehaviorTreeGraphNode* Child =
					Cast<UBehaviorTreeGraphNode>(Linked->GetOwningNode()))
				{
					Children.Add(Child);
				}
			}
		}
		return Children;
	}

	UBehaviorTreeGraphNode* GetParentNode(const UBehaviorTreeGraphNode* Node)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() > 0 && Pin->LinkedTo[0])
			{
				return Cast<UBehaviorTreeGraphNode>(Pin->LinkedTo[0]->GetOwningNode());
			}
		}
		return nullptr;
	}

	namespace
	{
		/** The name a path segment matches on. */
		FString NodeDisplayName(const UBehaviorTreeGraphNode* Node)
		{
			return Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : FString();
		}

		/**
		 * "Name[n]" for Node among Siblings, where n counts same-named siblings only.
		 *
		 * The index is always emitted, even when nothing currently shares the name. Resolution
		 * treats it as optional, so a hand-written "Root/Selector/Wait" still works; but a path this
		 * function hands back is one a caller is going to hold on to and use again, and a bare name
		 * stops resolving the moment a same-named sibling is added next to it — turning a path that
		 * was correct when it was issued into an "ambiguous" error two calls later. Emitting the
		 * index means a returned path is never ambiguous, whatever is added around it afterwards.
		 *
		 * It is still positional: the node a path names can change when siblings are inserted or
		 * removed before it. "guid" is the identity that survives that.
		 */
		FString SegmentFor(const UBehaviorTreeGraphNode* Node,
			const TArray<UBehaviorTreeGraphNode*>& Siblings)
		{
			const FString Name = NodeDisplayName(Node);

			int32 SameNamed = 0;
			int32 IndexAmongSameNamed = INDEX_NONE;
			for (const UBehaviorTreeGraphNode* Sibling : Siblings)
			{
				if (NodeDisplayName(Sibling) == Name)
				{
					if (Sibling == Node)
					{
						IndexAmongSameNamed = SameNamed;
					}
					++SameNamed;
				}
			}

			return (IndexAmongSameNamed != INDEX_NONE)
				? FString::Printf(TEXT("%s[%d]"), *Name, IndexAmongSameNamed)
				: Name;
		}

		/** Split "Name[3]" into ("Name", 3); a bare "Name" yields INDEX_NONE. */
		void SplitSegment(const FString& Segment, FString& OutName, int32& OutIndex)
		{
			OutName = Segment;
			OutIndex = INDEX_NONE;

			if (!Segment.EndsWith(TEXT("]")))
			{
				return;
			}
			int32 OpenIdx = INDEX_NONE;
			if (!Segment.FindLastChar(TEXT('['), OpenIdx))
			{
				return;
			}

			const FString IndexText = Segment.Mid(OpenIdx + 1, Segment.Len() - OpenIdx - 2);
			// A non-numeric bracket body is part of the name, not an index: refusing to guess keeps
			// a node genuinely called "Wait[a]" addressable.
			if (IndexText.IsEmpty() || !IndexText.IsNumeric())
			{
				return;
			}

			OutName = Segment.Left(OpenIdx);
			OutIndex = FCString::Atoi(*IndexText);
		}
	}

	FString GetNodePath(const UBehaviorTreeGraphNode* Node)
	{
		if (!Node)
		{
			return FString();
		}

		TArray<FString> Segments;

		// A sub-node is addressed by its slot on its owner, not through pins — decorators and
		// services have no pins at all — so it contributes one "@kind[n]" segment and then hands
		// over to the ordinary pin walk from its owner upwards.
		const UBehaviorTreeGraphNode* Current = Node;
		if (const UBehaviorTreeGraphNode* Owner =
			Cast<UBehaviorTreeGraphNode>(ToRawPtr(Node->ParentNode)))
		{
			UBehaviorTreeGraphNode* const Self = const_cast<UBehaviorTreeGraphNode*>(Node);
			const int32 DecoratorIdx = Owner->Decorators.IndexOfByKey(Self);
			const int32 ServiceIdx = Owner->Services.IndexOfByKey(Self);
			if (DecoratorIdx != INDEX_NONE)
			{
				Segments.Add(FString::Printf(TEXT("@decorator[%d]"), DecoratorIdx));
			}
			else if (ServiceIdx != INDEX_NONE)
			{
				Segments.Add(FString::Printf(TEXT("@service[%d]"), ServiceIdx));
			}
			else
			{
				// Parented but in neither array: not addressable, so say so rather than hand back
				// a path that resolves to the owner.
				return FString();
			}
			Current = Owner;
		}

		// Walk to the root, one segment per hop. The visited set makes a corrupted graph with a
		// parent cycle terminate instead of hanging.
		TSet<const UBehaviorTreeGraphNode*> Visited;
		while (Current && !Visited.Contains(Current))
		{
			Visited.Add(Current);

			const UBehaviorTreeGraphNode* Parent = GetParentNode(Current);
			if (!Parent)
			{
				break;
			}
			Segments.Add(SegmentFor(Current, GetChildNodes(Parent)));
			Current = Parent;
		}

		// Anything that did not walk up to the root node is unreachable from it — an orphan left by
		// a hand-edited graph, or a cycle — and has no path a caller could resolve.
		if (!Current || !Current->IsA<UBehaviorTreeGraphNode_Root>())
		{
			return FString();
		}

		Algo::Reverse(Segments);

		FString Path = TEXT("Root");
		for (const FString& Segment : Segments)
		{
			Path += TEXT("/");
			Path += Segment;
		}
		return Path;
	}

	UBehaviorTreeGraphNode* ResolveNodePath(UBehaviorTreeGraph* Graph, const FString& Path)
	{
		TArray<FString> Segments;
		Path.ParseIntoArray(Segments, TEXT("/"), /*InCullEmpty*/ true);
		if (Segments.Num() == 0)
		{
			return nullptr;
		}

		// The root segment is a keyword, not a name lookup: the root graph node's own title is
		// "ROOT" (BehaviorTreeGraphNode_Root.cpp), and the grammar says every path starts at "Root".
		if (!Segments[0].Equals(TEXT("Root"), ESearchCase::IgnoreCase))
		{
			return nullptr;
		}

		UBehaviorTreeGraphNode* Current = FindRootGraphNode(Graph);
		for (int32 Index = 1; Current && Index < Segments.Num(); ++Index)
		{
			const FString& Segment = Segments[Index];

			FString Name;
			int32 Ordinal = INDEX_NONE;
			SplitSegment(Segment, Name, Ordinal);

			if (Name.StartsWith(TEXT("@")))
			{
				// Sub-node slots are addressed by index alone — an unindexed "@decorator" names the
				// whole array, not a node.
				if (Ordinal == INDEX_NONE)
				{
					return nullptr;
				}
				const TArray<TObjectPtr<UBehaviorTreeGraphNode>>* Slots = nullptr;
				if (Name.Equals(TEXT("@decorator"), ESearchCase::IgnoreCase))
				{
					Slots = &Current->Decorators;
				}
				else if (Name.Equals(TEXT("@service"), ESearchCase::IgnoreCase))
				{
					Slots = &Current->Services;
				}
				if (!Slots || !Slots->IsValidIndex(Ordinal))
				{
					return nullptr;
				}
				Current = ToRawPtr((*Slots)[Ordinal]);
				continue;
			}

			TArray<UBehaviorTreeGraphNode*> Matches;
			for (UBehaviorTreeGraphNode* Child : GetChildNodes(Current))
			{
				if (NodeDisplayName(Child) == Name)
				{
					Matches.Add(Child);
				}
			}

			if (Ordinal != INDEX_NONE)
			{
				Current = Matches.IsValidIndex(Ordinal) ? Matches[Ordinal] : nullptr;
			}
			else
			{
				// Exactly one, or nothing: an unindexed segment matching several siblings is
				// ambiguous, and picking the first would silently edit the wrong node.
				Current = (Matches.Num() == 1) ? Matches[0] : nullptr;
			}
		}

		return Current;
	}
}
