// Copyright VibeUE. SPDX-License-Identifier: MIT
//
// Replacement for the upstream UFormatterSettings (UCLASS) from
// howaajin/graphformatter. We don't expose configuration to the editor
// UI (yet); the algorithm runs headlessly from Python tools, so we
// just supply the upstream defaults as a plain struct here.
//
// Naming the type UFormatterSettings keeps the vendored .cpp files
// nearly verbatim — only the include path and the GetDefault<>() call
// site change.

#pragma once

#include "CoreMinimal.h"
#include "FormatterGraph.h"

struct UFormatterSettings
{
    int32 HorizontalSpacing = 100;
    int32 VerticalSpacing = 80;
    int32 MaxLayerNodes = 5;
    int32 MaxOrderingIterations = 10;
    int32 CommentBorder = 45;
    bool  bEnableBlueprintParameterGroup = true;
    FVector2D SpacingFactorOfParameterGroup = FVector2D(0.314, 0.314);
    EGraphFormatterPositioningAlgorithm PositioningAlgorithm =
        EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodMedian;

    static const UFormatterSettings& Get()
    {
        static const UFormatterSettings Singleton;
        return Singleton;
    }
};
