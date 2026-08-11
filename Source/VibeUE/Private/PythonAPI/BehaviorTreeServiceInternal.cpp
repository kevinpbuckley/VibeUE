// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "BehaviorTreeServiceInternal.h"

#include "AIGraphTypes.h"
#include "BehaviorTree/BehaviorTree.h"
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

		/** Children of a BT graph node, in current output-pin link order. Skips already-visited nodes to prevent cycles. */
		void GatherChildren(UBehaviorTreeGraphNode* Node, TArray<UBehaviorTreeGraphNode*>& Out,
			const TSet<UBehaviorTreeGraphNode*>& Visited)
		{
			Out.Reset();
			if (!Node)
			{
				return;
			}
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Output)
				{
					for (UEdGraphPin* Linked : Pin->LinkedTo)
					{
						if (Linked)
						{
							if (UBehaviorTreeGraphNode* Child =
								Cast<UBehaviorTreeGraphNode>(Linked->GetOwningNode()))
							{
								if (!Visited.Contains(Child))
								{
									Out.Add(Child);
								}
							}
						}
					}
				}
			}
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

	namespace
	{
		/**
		 * The graph's root node, or nullptr. A linear sweep of Nodes is the whole search: BT
		 * sub-nodes (decorators, services) live in UAIGraphNode::SubNodes and are never added to
		 * UEdGraph::Nodes (AIGraphNode.cpp, UAIGraphNode::AddSubNode).
		 */
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
			UBlackboardData* const OriginalBlackboard = Tree->BlackboardAsset;

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

			Root->BlackboardAsset = OriginalBlackboard;
			Tree->BlackboardAsset = OriginalBlackboard;
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

		// Checked before EnsureGraph, so a refused write leaves the asset exactly as it was.
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

		if (Graph->IsLocked())
		{
			return FString::Printf(
				TEXT("Graph updates are locked on %s (bLockUpdates). UBehaviorTreeGraph::UpdateAsset() "
					 "early-returns while it is set, so the write would be saved with a stale runtime "
					 "tree and still report success."),
				*AssetPath);
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

		// Never UBehaviorTreeGraph::AutoArrange(): it dereferences RootNode->DEPRECATED_NodeWidget
		// (BehaviorTreeGraph.cpp:1302), the Slate widget, which is only ever set while a Behavior
		// Tree editor tab is open — it crashes the editor outright when called headlessly.
		ArrangeGraph(Root);

		// OnSave() is SpawnMissingNodesForParallel() + UpdateAsset(): exactly what the BT editor runs
		// on save, and what regenerates UBehaviorTree::RootNode from the graph. A bare UpdateAsset()
		// would skip the parallel-task fix-up.
		Graph->OnSave();

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
}
