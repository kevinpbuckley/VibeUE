/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "EvenlyPlaceStrategy.h"
#include "FormatterGraph.h"
#include "VibeUEFormatterSettings.h"

FBox2D FEvenlyPlaceStrategy::PlaceNodeInLayer(TArray<FFormatterNode*>& Layer, const FBox2D& PreBound)
{
    FBox2D Bound(ForceInit);
    const UFormatterSettings& Settings = UFormatterSettings::Get();
    FVector2D Position;
    if (PreBound.bIsValid)
    {
        Position = FVector2D(PreBound.Max.X + Settings.HorizontalSpacing, 0);
    }
    else
    {
        Position = FVector2D(0, 0);
    }
    for (auto Node : Layer)
    {
        if (Node->OriginalNode == nullptr)
        {
            Node->SetPosition(Position);
            continue;
        }
        Node->SetPosition(Position);
        if (Bound.bIsValid)
        {
            Bound += FBox2D(Position, Position + Node->Size);
        }
        else
        {
            Bound = FBox2D(Position, Position + Node->Size);
        }
        Position.Y += Node->Size.Y + Settings.VerticalSpacing;
    }
    return Bound;
}

FFormatterNode* FEvenlyPlaceStrategy::FindFirstNodeInLayeredList(TArray<TArray<FFormatterNode*>>& InLayeredNodes)
{
    for (const auto& Layer : InLayeredNodes)
    {
        for (auto Node : Layer)
        {
            return Node;
        }
    }
    return nullptr;
}

FEvenlyPlaceStrategy::FEvenlyPlaceStrategy(TArray<TArray<FFormatterNode*>>& InLayeredNodes)
    : IPositioningStrategy(InLayeredNodes)
{
    FVector2D StartPosition;
    FFormatterNode* FirstNode = FindFirstNodeInLayeredList(InLayeredNodes);
    if (FirstNode != nullptr)
    {
        StartPosition = FirstNode->GetPosition();
    }

    float MaxHeight = 0;
    FBox2D PreBound(ForceInit);
    TArray<FBox2D> Bounds;
    for (auto& Layer : InLayeredNodes)
    {
        PreBound = PlaceNodeInLayer(Layer, PreBound);
        Bounds.Add(PreBound);
        if (TotalBound.bIsValid)
        {
            TotalBound += PreBound;
        }
        else
        {
            TotalBound = PreBound;
        }
        const float Height = PreBound.GetSize().Y;
        if (Height > MaxHeight)
        {
            MaxHeight = Height;
        }
    }

    StartPosition -= FVector2D(0, MaxHeight - Bounds[0].GetSize().Y) / 2.0f;

    for (int32 i = 0; i < InLayeredNodes.Num(); i++)
    {
        const FVector2D Offset = FVector2D(0, (MaxHeight - Bounds[i].GetSize().Y) / 2.0f) + StartPosition;
        for (auto Node : InLayeredNodes[i])
        {
            Node->SetPosition(Node->GetPosition() + Offset);
        }
    }
    TotalBound = FBox2D(StartPosition, StartPosition + TotalBound.GetSize());
}
