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
			// Centre over the span, doubling first so an even span still lands on an integer
			// that preserves strict ordering between siblings.
			Out[SelfIndex].X = (FirstColumn + LastColumn) * NodeSpacingX / 2;
			Out[SelfIndex].Y = Depth * NodeSpacingY;
		}

		/** Children of a BT graph node, in current output-pin link order. */
		void GatherChildren(UBehaviorTreeGraphNode* Node, TArray<UBehaviorTreeGraphNode*>& Out)
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
								Out.Add(Child);
							}
						}
					}
				}
			}
		}

		/** Build the structure mirror and, in the same pre-order, the node list to write back. */
		void BuildMirror(UBehaviorTreeGraphNode* Node, FLayoutNode& OutNode,
			TArray<UBehaviorTreeGraphNode*>& OutOrder)
		{
			OutOrder.Add(Node);

			TArray<UBehaviorTreeGraphNode*> Children;
			GatherChildren(Node, Children);

			OutNode.Children.AddDefaulted(Children.Num());
			for (int32 Index = 0; Index < Children.Num(); ++Index)
			{
				BuildMirror(Children[Index], OutNode.Children[Index], OutOrder);
			}
		}
	}

	TArray<FIntPoint> ComputeLayout(const FLayoutNode& Root)
	{
		TArray<FIntPoint> Positions;
		Positions.Reserve(CountLeafColumns(Root) * 2);

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
		BuildMirror(RootNode, Mirror, Order);

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
