// Copyright VibeUE 2025

#pragma once

#include "CoreMinimal.h"

/**
 * Blueprint Property Type Definitions
 * 
 * This header contains data structures related to Blueprint properties
 * and property metadata.
 */

/**
 * Property information structure
 */
struct VIBEUE_API FPropertyInfo
{
    FString PropertyName;
    FString PropertyType;
    FString PropertyClass;
    FString Category;
    FString Tooltip;
    FString CurrentValue;
    FString DefaultValue;
    bool bIsEditable;
    bool bIsBlueprintVisible;
    bool bIsBlueprintReadOnly;
    
    // Type-specific metadata
    FString MinValue;
    FString MaxValue;
    FString UIMin;
    FString UIMax;
    FString ObjectClass;
    FString ObjectValue;
    
    /** Available values for enum properties */
    TArray<FString> EnumValues;

    FPropertyInfo()
        : bIsEditable(false)
        , bIsBlueprintVisible(false)
        , bIsBlueprintReadOnly(false)
    {}
};
