/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "PriorityPositioningStrategy.h"
#include "FormatterGraph.h"
#include "VibeUEFormatterSettings.h"
#include "EvenlyPlaceStrategy.h"

static FBox2D PlaceNodeInLayer(TArray<FFormatterNode*>& Layer)
{
    FBox2D Bound(ForceInit);
    FVector2D Position = FVector2D(0, 0);
    for (auto Node : Layer)
    {
        Node->SetPosition(Position);
        if (Bound.bIsValid)
        {
            Bound += FBox2D(Position, Position + Node->Size);
        }
        else
        {
            Bound = FBox2D(Position, Position + Node->Size);
        }
        Position.Y += Node->Size.Y;
    }
    return Bound;
}

static bool GetBarycenter(FFormatterNode* Node, EEdGraphPinDirection Direction, float& Barycenter)
{
    float ToLayerYSum = 0.0f;
    float SelfYSum = 0.0f;
    const auto& Edges = Direction == EGPD_Output ? Node->OutEdges : Node->InEdges;
    if (Edges.Num() == 0)
    {
        Barycenter = Node->GetPosition().Y;
        return false;
    }
    for (auto Edge : Edges)
    {
        const auto LinkedToNode = Edge->To->OwningNode;
        const auto LinkedToPosition = LinkedToNode->GetPosition() + Edge->To->NodeOffset;
        ToLayerYSum += LinkedToPosition.Y;
        SelfYSum += Edge->From->NodeOffset.Y;
    }
    const float ToAverage = ToLayerYSum / Edges.Num();
    const float FromAverage = SelfYSum / Edges.Num();
    Barycenter = ToAverage - FromAverage;
    return true;
}

static bool GetClosestPositionToBarycenter(const TArray<FFormatterNode*>& Slots, int32 Previous, int32 Next, FFormatterNode* Node, float& Barycenter, float& Y)
{
    const UFormatterSettings& Settings = UFormatterSettings::Get();
    FFormatterNode* PreviousNode = nullptr;
    bool bUpFree = false;
    if (Previous < 0 || Slots[Previous] == nullptr)
    {
        bUpFree = true;
    }
    else
    {
        PreviousNode = Slots[Previous];
    }
    FFormatterNode* NextNode = nullptr;
    bool bDownFree = false;
    if (Next >= Slots.Num() || Slots[Next] == nullptr)
    {
        bDownFree = true;
    }
    else
    {
        NextNode = Slots[Next];
    }
    if (bUpFree && bDownFree)
    {
        Y = Barycenter;
        return true;
    }
    if (bUpFree)
    {
        const float MostDown = NextNode->GetPosition().Y - Node->Size.Y - Settings.VerticalSpacing;
        Y = FMath::Min(Barycenter, MostDown);
        return true;
    }
    if (bDownFree)
    {
        const float MostUp = PreviousNode->GetPosition().Y + PreviousNode->Size.Y + Settings.VerticalSpacing;
        Y = FMath::Max(MostUp, Barycenter);
        return true;
    }
    const float MostDown = NextNode->GetPosition().Y - Node->Size.Y - Settings.VerticalSpacing;
    const float MostUp = PreviousNode->GetPosition().Y + PreviousNode->Size.Y + Settings.VerticalSpacing;
    if (MostDown < MostUp)
    {
        Y = MostUp - MostDown;
        Barycenter = MostDown + Y / 2.0f;
        return false;
    }
    Y = FMath::Clamp(Barycenter, MostUp, MostDown);
    return true;
}

static void ShiftInLayer(TArray<FFormatterNode*> Slots, int32 Index, float Distance)
{
    for (int32 i = 0; i < Index; i++)
    {
        auto Node = Slots[i];
        if (Node != nullptr)
        {
            Node->SetPosition(Node->GetPosition() - FVector2D(0, Distance));
        }
    }
    for (int32 i = Index + 1; i < Slots.Num(); i++)
    {
        auto Node = Slots[i];
        if (Node != nullptr)
        {
            Node->SetPosition(Node->GetPosition() + FVector2D(0, Distance));
        }
    }
}

static void PositioningSweep(TArray<TArray<FFormatterNode*>>& InLayeredNodes, EEdGraphPinDirection Direction, const TArray<FBox2D>& LayersBound)
{
    int32 StartIndex, EndIndex, Step;
    if (Direction == EGPD_Input)
    {
        StartIndex = 1;
        EndIndex = InLayeredNodes.Num();
        Step = 1;
    }
    else
    {
        StartIndex = InLayeredNodes.Num() - 2;
        EndIndex = -1;
        Step = -1;
    }
    for (int32 i = StartIndex; i != EndIndex; i += Step)
    {
        auto CurrentLayer = InLayeredNodes[i];
        auto PreviousLayer = InLayeredNodes[i - Step];
        TArray<FFormatterNode*> Slots;
        Slots.SetNumZeroed(CurrentLayer.Num());
        TArray<FFormatterNode*> PriorityList = CurrentLayer;
        for (auto Node : CurrentLayer)
        {
            Node->PositioningPriority = Node->CalcPriority(Direction);
        }
        PriorityList.Sort([](const FFormatterNode& A, const FFormatterNode& B)
        {
            return A.PositioningPriority > B.PositioningPriority;
        });
        FVector2D Position;
        for (auto Node : PriorityList)
        {
            float Barycenter;
            const bool IsConnected = GetBarycenter(Node, Direction, Barycenter);
            if (IsConnected)
            {
                Position = LayersBound[i].Min;
            }
            else
            {
                Position = LayersBound[i].Max - FVector2D(Node->Size.X, 0);
            }
            const int32 Index = CurrentLayer.Find(Node);
            const int32 Previous = Index - 1;
            const int32 Next = Index + 1;
            float OutY;
            if (GetClosestPositionToBarycenter(Slots, Previous, Next, Node, Barycenter, OutY))
            {
                Position.Y = OutY;
            }
            else
            {
                ShiftInLayer(Slots, Index, OutY / 2.0f);
                Position.Y = Barycenter;
            }
            Slots[Index] = Node;
            Node->SetPosition(Position);
        }
    }
}

FPriorityPositioningStrategy::FPriorityPositioningStrategy(TArray<TArray<FFormatterNode*>>& InLayeredNodes)
    : IPositioningStrategy(InLayeredNodes)
{
    if (InLayeredNodes.Num() < 2)
    {
        FEvenlyPlaceStrategy LeftToRightPositioningStrategy(InLayeredNodes);
        TotalBound = LeftToRightPositioningStrategy.GetTotalBound();
        return;
    }
    ensure(InLayeredNodes[0].Num() > 0);
    FFormatterNode* FirstNode = InLayeredNodes[0][0];
    const FVector2D OldPosition = FirstNode->GetPosition();
    const auto LayersBound = FFormatterGraph::CalculateLayersBound(InLayeredNodes);
    for (auto& Layer : InLayeredNodes)
    {
        PlaceNodeInLayer(Layer);
    }
    PositioningSweep(InLayeredNodes, EGPD_Input, LayersBound);
    PositioningSweep(InLayeredNodes, EGPD_Output, LayersBound);
    PositioningSweep(InLayeredNodes, EGPD_Input, LayersBound);

    FBox2D Bound(ForceInit);
    const FVector2D NewPosition = FirstNode->GetPosition();
    for (int32 i = 0; i < InLayeredNodes.Num(); i++)
    {
        const FVector2D Offset = OldPosition - NewPosition;
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
