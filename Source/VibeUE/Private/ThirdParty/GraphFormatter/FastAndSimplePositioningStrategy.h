/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "IPositioningStrategy.h"

class FFastAndSimplePositioningStrategy : public IPositioningStrategy
{
    TMap<FFormatterNode*, FFormatterNode*> ConflictMarks;
    TMap<FFormatterNode*, FFormatterNode*> RootMap;
    TMap<FFormatterNode*, FFormatterNode*> AlignMap;
    TMap<FFormatterNode*, FFormatterNode*> SinkMap;
    TMap<FFormatterNode*, float> ShiftMap;
    TMap<FFormatterNode*, float> InnerShiftMap;
    TMap<FFormatterNode*, float>* XMap;
    TMap<FFormatterNode*, int32> PosMap;
    TMap<FFormatterNode*, float> BlockWidthMap;
    TMap<FFormatterNode*, FFormatterNode*> PredecessorMap;
    TMap<FFormatterNode*, FFormatterNode*> SuccessorMap;
    TMap<FFormatterNode*, float> UpperLeftPositionMap;
    TMap<FFormatterNode*, float> UpperRightPositionMap;
    TMap<FFormatterNode*, float> LowerLeftPositionMap;
    TMap<FFormatterNode*, float> LowerRightPositionMap;
    TMap<FFormatterNode*, float> CombinedPositionMap;
    bool IsUpperDirection;
    bool IsLeftDirection;
    bool IsHorizontalDirection;
    bool IsParameterGroup;
    void Initialize();
    void MarkConflicts();
    void DoVerticalAlignment();
    void DoHorizontalCompaction();
    void PlaceBlock(FFormatterNode* BlockRoot);
    void CalculateInnerShift();
    void Sweep();
    void Combine();
    void DoOnePass();
public:
    explicit FFastAndSimplePositioningStrategy(TArray<TArray<FFormatterNode*>>& InLayeredNodes, bool IsHorizontalDirection = true, bool IsParameterGroup = false);
};
