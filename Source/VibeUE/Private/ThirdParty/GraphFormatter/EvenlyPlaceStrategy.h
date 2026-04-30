/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"
#include "IPositioningStrategy.h"

class FEvenlyPlaceStrategy : public IPositioningStrategy
{
    FBox2D PlaceNodeInLayer(TArray<FFormatterNode*>& Layer, const FBox2D& PreBound);
    FFormatterNode* FindFirstNodeInLayeredList(TArray<TArray<FFormatterNode*>>& InLayeredNodes);
public:
    explicit FEvenlyPlaceStrategy(TArray<TArray<FFormatterNode*>>& InLayeredNodes);
};
