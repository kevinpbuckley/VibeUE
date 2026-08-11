// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "BehaviorTreeServiceInternal.h"

#include "BehaviorTreeGraphNode.h"
#include "EdGraph/EdGraphPin.h"

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
}
