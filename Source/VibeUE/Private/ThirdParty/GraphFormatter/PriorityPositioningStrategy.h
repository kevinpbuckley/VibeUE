/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"
#include "IPositioningStrategy.h"

class FPriorityPositioningStrategy : public IPositioningStrategy
{
public:
    explicit FPriorityPositioningStrategy(TArray<TArray<FFormatterNode*>>& InLayeredNodes);
};
