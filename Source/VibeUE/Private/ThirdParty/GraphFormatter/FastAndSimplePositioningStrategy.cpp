/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FastAndSimplePositioningStrategy.h"
#include "FormatterGraph.h"
#include "VibeUEFormatterSettings.h"

void FFastAndSimplePositioningStrategy::Initialize()
{
    for (auto& Layer : LayeredNodes)
    {
        for (int32 i = 0; i < Layer.Num(); i++)
        {
            PosMap.Add(Layer[i], i);
            if (i != 0)
            {
                PredecessorMap.Add(Layer[i], Layer[i - 1]);
            }
            else
            {
                PredecessorMap.Add(Layer[i], nullptr);
            }
            if (i != Layer.Num() - 1)
            {
                SuccessorMap.Add(Layer[i], Layer[i + 1]);
            }
            else
            {
                SuccessorMap.Add(Layer[i], nullptr);
            }
        }
    }
    MarkConflicts();
}

void FFastAndSimplePositioningStrategy::MarkConflicts()
{
    for (int32 i = 1; i < LayeredNodes.Num() - 1; i++)
    {
        int32 k0 = 0;
        int32 l = 1;
        for (int32 l1 = 0; l1 < LayeredNodes[i + 1].Num(); l1++)
        {
            auto Node = LayeredNodes[i + 1][l1];
            bool IsCrossingInnerSegment = Node->IsCrossingInnerSegment(LayeredNodes[i + 1], LayeredNodes[i]);
            if (l1 == LayeredNodes[i + 1].Num() - 1 || IsCrossingInnerSegment)
            {
                int32 k1 = LayeredNodes[i].Num();
                if (IsCrossingInnerSegment)
                {
                    const auto MedianUpper = Node->GetMedianUpper();
                    k1 = PosMap[MedianUpper];
                }
                while (l < l1)
                {
                    auto UpperNodes = Node->GetUppers();
                    for (auto UpperNode : UpperNodes)
                    {
                        auto k = PosMap[UpperNode];
                        if (k < k0 || k > k1)
                        {
                            ConflictMarks.Add(UpperNode, Node);
                        }
                    }
                    ++l;
                }
                k0 = k1;
            }
        }
    }
}

void FFastAndSimplePositioningStrategy::DoVerticalAlignment()
{
    RootMap.Empty();
    AlignMap.Empty();
    for (auto Layer : LayeredNodes)
    {
        for (auto Node : Layer)
        {
            RootMap.Add(Node, Node);
            AlignMap.Add(Node, Node);
        }
    }
    int32 LayerStep = IsUpperDirection ? 1 : -1;
    int32 LayerStart = IsUpperDirection ? 0 : LayeredNodes.Num() - 1;
    int32 LayerEnd = IsUpperDirection ? LayeredNodes.Num() : -1;
    for (int32 i = LayerStart; i != LayerEnd; i += LayerStep)
    {
        float Weight = 0.0f;
        int32 Guide = IsLeftDirection ? -1 : INT_MAX;
        int32 Step = IsLeftDirection ? 1 : -1;
        int32 Start = IsLeftDirection ? 0 : LayeredNodes[i].Num() - 1;
        int32 End = IsLeftDirection ? LayeredNodes[i].Num() : -1;
        for (int32 k = Start; k != End; k += Step)
        {
            auto Node = LayeredNodes[i][k];
            auto Adjacencies = IsUpperDirection ? Node->GetUppers() : Node->GetLowers();
            if (Adjacencies.Num() > 0)
            {
                int32 ma = FMath::TruncToInt((Adjacencies.Num() + 1) / 2.0f - 1);
                int32 mb = FMath::CeilToInt((Adjacencies.Num() + 1) / 2.0f - 1);
                for (int32 m = ma; m <= mb; m++)
                {
                    if (AlignMap[Node] == Node)
                    {
                        auto& MedianNode = Adjacencies[m];
                        bool IsMarked = ConflictMarks.Contains(MedianNode) && ConflictMarks[MedianNode] == Node;
                        float MaxWeight = MedianNode->GetMaxWeight(IsUpperDirection ? EGPD_Output : EGPD_Input);
                        float LinkWeight = Node->GetMaxWeightToNode(MedianNode, IsUpperDirection ? EGPD_Input : EGPD_Output);
                        const auto MedianNodePos = PosMap[MedianNode];
                        bool IsGuideAccepted = IsLeftDirection ? MedianNodePos > Guide : MedianNodePos < Guide;
                        if (!IsMarked)
                        {
                            if (IsGuideAccepted && LinkWeight == MaxWeight)
                            {
                                AlignMap[MedianNode] = Node;
                                RootMap[Node] = RootMap[MedianNode];
                                AlignMap[Node] = RootMap[Node];
                                Guide = MedianNodePos;
                            }
                        }
                    }
                }
            }
        }
    }
}

void FFastAndSimplePositioningStrategy::DoHorizontalCompaction()
{
    SinkMap.Empty();
    ShiftMap.Empty();
    XMap->Empty();
    for (auto& Layer : LayeredNodes)
    {
        for (auto Node : Layer)
        {
            SinkMap.Add(Node, Node);
            if (IsLeftDirection)
            {
                ShiftMap.Add(Node, FLT_MAX);
            }
            else
            {
                ShiftMap.Add(Node, -FLT_MAX);
            }
            XMap->Add(Node, NAN);
        }
    }
    for (auto& Layer : LayeredNodes)
    {
        for (auto Node : Layer)
        {
            if (RootMap[Node] == Node)
            {
                PlaceBlock(Node);
            }
        }
    }
    for (auto& Layer : LayeredNodes)
    {
        for (auto Node : Layer)
        {
            auto& RootNode = RootMap[Node];
            (*XMap)[Node] = (*XMap)[RootNode];
        }
    }
    for (auto& Layer : LayeredNodes)
    {
        for (auto Node : Layer)
        {
            auto& RootNode = RootMap[Node];
            const float Shift = ShiftMap[SinkMap[RootNode]];
            if ((IsLeftDirection && Shift < FLT_MAX) || (!IsLeftDirection && Shift > -FLT_MAX))
            {
                (*XMap)[Node] = (*XMap)[Node] + Shift;
            }
        }
    }
    for (auto& Layer : LayeredNodes)
    {
        for (auto Node : Layer)
        {
            (*XMap)[Node] += InnerShiftMap[Node];
        }
    }
}

void FFastAndSimplePositioningStrategy::PlaceBlock(FFormatterNode* BlockRoot)
{
    const UFormatterSettings& Settings = UFormatterSettings::Get();
    if (FMath::IsNaN((*XMap)[BlockRoot]))
    {
        bool Initial = true;
        (*XMap)[BlockRoot] = 0;
        auto Node = BlockRoot;
        do
        {
            const auto Adjacency = IsLeftDirection ? PredecessorMap[Node] : SuccessorMap[Node];
            if (Adjacency != nullptr)
            {
                float AdjacencyHeight, NodeHeight;
                float Spacing;
                if (IsHorizontalDirection)
                {
                    AdjacencyHeight = Adjacency->Size.Y;
                    NodeHeight = Node->Size.Y;
                    Spacing = Settings.VerticalSpacing;
                    if(IsParameterGroup)
                    {
                        Spacing *= Settings.SpacingFactorOfParameterGroup.Y;
                    }
                }
                else
                {
                    AdjacencyHeight = Adjacency->Size.X;
                    NodeHeight = Node->Size.X;
                    Spacing = Settings.HorizontalSpacing;
                    if (IsParameterGroup)
                    {
                        Spacing *= Settings.SpacingFactorOfParameterGroup.X;
                    }
                }

                const auto PrevBlockRoot = RootMap[Adjacency];
                PlaceBlock(PrevBlockRoot);
                if (SinkMap[BlockRoot] == BlockRoot)
                {
                    SinkMap[BlockRoot] = SinkMap[PrevBlockRoot];
                }
                if (SinkMap[BlockRoot] != SinkMap[PrevBlockRoot])
                {
                    float LeftShift = (*XMap)[BlockRoot] - (*XMap)[PrevBlockRoot] + InnerShiftMap[Node] - InnerShiftMap[Adjacency] - AdjacencyHeight - Spacing;
                    float RightShift = (*XMap)[BlockRoot] - (*XMap)[PrevBlockRoot] - InnerShiftMap[Node] + InnerShiftMap[Adjacency] + NodeHeight + Spacing;
                    float Shift = IsLeftDirection ? FMath::Min(ShiftMap[SinkMap[PrevBlockRoot]], LeftShift) : FMath::Max(ShiftMap[SinkMap[PrevBlockRoot]], RightShift);
                    ShiftMap[SinkMap[PrevBlockRoot]] = Shift;
                }
                else
                {
                    float LeftShift = InnerShiftMap[Adjacency] + AdjacencyHeight - InnerShiftMap[Node] + Spacing;
                    float RightShift = -NodeHeight - Spacing + InnerShiftMap[Adjacency] - InnerShiftMap[Node];
                    float Shift = IsLeftDirection ? LeftShift : RightShift;
                    float Position = (*XMap)[PrevBlockRoot] + Shift;
                    if (Initial)
                    {
                        (*XMap)[BlockRoot] = Position;
                        Initial = false;
                    }
                    else
                    {
                        Position = IsLeftDirection ? FMath::Max((*XMap)[BlockRoot], Position) : FMath::Min((*XMap)[BlockRoot], Position);
                        (*XMap)[BlockRoot] = Position;
                    }
                }
            }
            Node = AlignMap[Node];
        }
        while (Node != BlockRoot);
    }
}

void FFastAndSimplePositioningStrategy::CalculateInnerShift()
{
    InnerShiftMap.Empty();
    BlockWidthMap.Empty();
    for (auto& Layer : LayeredNodes)
    {
        for (auto Node : Layer)
        {
            if (RootMap[Node] == Node)
            {
                InnerShiftMap.Add(Node, 0);
                float Left = 0, Right = IsHorizontalDirection ? Node->Size.Y : Node->Size.X;
                auto RootNode = Node;
                auto UpperNode = Node;
                auto LowerNode = AlignMap[RootNode];
                while (true)
                {
                    const float UpperPosition = UpperNode->GetLinkedPositionToNode(LowerNode, IsUpperDirection ? EGPD_Output : EGPD_Input, IsHorizontalDirection);
                    const float LowerPosition = LowerNode->GetLinkedPositionToNode(UpperNode, IsUpperDirection ? EGPD_Input : EGPD_Output, IsHorizontalDirection);
                    const float Shift = InnerShiftMap[UpperNode] + UpperPosition - LowerPosition;
                    InnerShiftMap.FindOrAdd(LowerNode) = Shift;
                    Left = FMath::Min(Left, Shift);
                    Right = FMath::Max(Right, Shift + (IsHorizontalDirection ? LowerNode->Size.Y : LowerNode->Size.X));
                    UpperNode = LowerNode;
                    LowerNode = AlignMap[UpperNode];
                    if (LowerNode == RootNode)
                    {
                        break;
                    }
                }
                auto CheckNode = Node;
                do
                {
                    InnerShiftMap[CheckNode] -= Left;
                    CheckNode = AlignMap[CheckNode];
                }
                while (CheckNode != Node);
                BlockWidthMap.FindOrAdd(Node) = Right - Left;
            }
        }
    }
}

void FFastAndSimplePositioningStrategy::Sweep()
{
    IsUpperDirection = true;
    IsLeftDirection = true;
    XMap = &UpperLeftPositionMap;
    DoOnePass();

    IsUpperDirection = true;
    IsLeftDirection = false;
    XMap = &UpperRightPositionMap;
    DoOnePass();

    IsUpperDirection = false;
    IsLeftDirection = true;
    XMap = &LowerLeftPositionMap;
    DoOnePass();

    IsUpperDirection = false;
    IsLeftDirection = false;
    XMap = &LowerRightPositionMap;
    DoOnePass();

    Combine();
}

void FFastAndSimplePositioningStrategy::Combine()
{
    TArray<TMap<FFormatterNode*, float>> Layouts = {UpperLeftPositionMap, UpperRightPositionMap, LowerLeftPositionMap, LowerRightPositionMap};
    TArray<TTuple<float, float>> Bounds;
    Bounds.SetNumUninitialized(Layouts.Num());
    int32 MinWidthIndex = -1;
    float MinWidth = FLT_MAX;
    for (int32 i = 0; i < Layouts.Num(); i++)
    {
        auto& Layout = Layouts[i];
        float LeftMost = FLT_MAX, RightMost = -FLT_MAX;
        for (auto& Pair : Layout)
        {
            if (Pair.Value < LeftMost)
            {
                LeftMost = Pair.Value;
            }
            if (Pair.Value > RightMost)
            {
                RightMost = Pair.Value;
            }
        }
        if (RightMost - LeftMost < MinWidth)
        {
            MinWidth = RightMost - LeftMost;
            MinWidthIndex = i;
        }
        Bounds[i] = TTuple<float, float>(LeftMost, RightMost);
    }
    for (int32 i = 0; i < Layouts.Num(); i++)
    {
        if (i != MinWidthIndex)
        {
            float Offset = Bounds[MinWidthIndex].Get<0>() - Bounds[i].Get<0>();
            for (auto& Pair : Layouts[i])
            {
                Pair.Value += Offset;
            }
        }
    }
    const UFormatterSettings& Settings = UFormatterSettings::Get();
    for (auto& Layer : LayeredNodes)
    {
        for (auto Node : Layer)
        {
            TArray Values = {Layouts[0][Node], Layouts[1][Node], Layouts[2][Node], Layouts[3][Node]};
            Values.Sort();
            if (!IsHorizontalDirection)
            {
                CombinedPositionMap.Add(Node, (Values[0] + Values[3]) / 2.0f);
            }
            else
            {
                if (Settings.PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodTop)
                {
                    CombinedPositionMap.Add(Node, Values[MinWidthIndex]);
                }
                else if (Settings.PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodMedian)
                {
                    CombinedPositionMap.Add(Node, (Values[2] + Values[3]) / 2.0f);
                }
            }
        }
    }
    XMap = &CombinedPositionMap;
}

void FFastAndSimplePositioningStrategy::DoOnePass()
{
    DoVerticalAlignment();
    CalculateInnerShift();
    DoHorizontalCompaction();
}

FFastAndSimplePositioningStrategy::FFastAndSimplePositioningStrategy(TArray<TArray<FFormatterNode*>>& InLayeredNodes, bool IsHorizontalDirection, bool IsParameterGroup)
    : IPositioningStrategy(InLayeredNodes)
    , IsHorizontalDirection(IsHorizontalDirection)
    , IsParameterGroup(IsParameterGroup)
{
    const auto LayersBound = FFormatterGraph::CalculateLayersBound(InLayeredNodes, IsHorizontalDirection, IsParameterGroup);
    FFormatterNode* FirstNode = InLayeredNodes[0][0];
    const FVector2D OldPosition = FirstNode->GetPosition();
    Initialize();
    Sweep();
    for (int i = 0; i < LayeredNodes.Num(); i++)
    {
        auto& Layer = LayeredNodes[i];
        for (auto Node : Layer)
        {
            float X, Y;
            float *pX, *pY;
            if (IsHorizontalDirection)
            {
                pX = &X;
                pY = &Y;
            }
            else
            {
                pX = &Y;
                pY = &X;
            }
            if (Node->InEdges.Num() == 0)
            {
                if (IsHorizontalDirection)
                {
                    *pX = LayersBound[i].Max.X - Node->Size.X;
                }
                else
                {
                    *pX = LayersBound[i].Max.Y - Node->Size.Y;
                }
            }
            else
            {
                if (IsHorizontalDirection)
                {
                    *pX = LayersBound[i].Min.X;
                }
                else
                {
                    *pX = LayersBound[i].Min.Y;
                }
            }
            *pY = (*XMap)[Node];
            Node->SetPosition(FVector2D(X, Y));
        }
    }
    const FVector2D NewPosition = FirstNode->GetPosition();
    const FVector2D Offset = OldPosition - NewPosition;
    FBox2D Bound(ForceInit);
    for (int32 i = 0; i < InLayeredNodes.Num(); i++)
    {
        for (auto Node : InLayeredNodes[i])
        {
            Node->SetPosition(Node->GetPosition() + Offset);
            if (Bound.bIsValid)
            {
                Bound += FBox2D(Node->GetPosition(), Node->GetPosition() + Node->Size);
            }
            else
            {
                Bound = FBox2D(Node->GetPosition(), Node->GetPosition() + Node->Size);
            }
        }
    }
    TotalBound = Bound;
}
