// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "BehaviorTreeServiceInternal.h"

#include "AIGraphTypes.h"
#include "BehaviorTreeGraphNode.h"
#include "EdGraph/EdGraphPin.h"
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
