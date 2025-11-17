// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Common Blueprint Type Definitions
 * 
 * This header contains commonly used Blueprint data structures
 * that are shared across multiple services.
 */

/**
 * Structure for class information
 */
struct VIBEUE_API FClassInfo
{
    FString ClassName;
    FString ClassPath;
    FString ParentClass;
    bool bIsAbstract;
    bool bIsBlueprint;
    
    FClassInfo()
        : bIsAbstract(false)
        , bIsBlueprint(false)
    {
    }
};

/**
 * @struct FBlueprintInfo
 * @brief Structure holding basic blueprint information
 * 
 * Contains essential metadata about a blueprint asset including its name,
 * path, parent class, and type information.
 */
struct VIBEUE_API FBlueprintInfo
{
    /** The name of the blueprint asset */
    FString Name;
    
    /** Full object path to the blueprint */
    FString Path;
    
    /** Package path containing the blueprint */
    FString PackagePath;
    
    /** Name of the parent class */
    FString ParentClass;
    
    /** Blueprint class type (e.g., "Blueprint", "WidgetBlueprint") */
    FString BlueprintType;
    
    /** True if this is a UMG Widget Blueprint */
    bool bIsWidgetBlueprint;

    FBlueprintInfo()
        : bIsWidgetBlueprint(false)
    {}
};
