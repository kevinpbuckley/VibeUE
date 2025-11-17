// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Blueprint Function Type Definitions
 * 
 * This header contains data structures related to Blueprint functions,
 * parameters, and local variables.
 */

/**
 * Information about a function parameter
 */
struct VIBEUE_API FFunctionParameterInfo
{
    FString Name;
    FString Direction;  // "input", "out", "return"
    FString Type;
    
    FFunctionParameterInfo() = default;
    FFunctionParameterInfo(const FString& InName, const FString& InDirection, const FString& InType)
        : Name(InName), Direction(InDirection), Type(InType) {}
};

/**
 * Information about a Blueprint function
 */
struct VIBEUE_API FFunctionInfo
{
    FString Name;
    FString GraphGuid;
    int32 NodeCount;
    
    FFunctionInfo() : NodeCount(0) {}
    FFunctionInfo(const FString& InName, const FString& InGraphGuid, int32 InNodeCount)
        : Name(InName), GraphGuid(InGraphGuid), NodeCount(InNodeCount) {}
};

/**
 * Information about a local variable
 */
struct VIBEUE_API FLocalVariableInfo
{
    FString Name;
    FString FriendlyName;
    FString Type;
    FString DisplayType;
    FString DefaultValue;
    FString Category;
    FString PinCategory;
    FString Guid;
    bool bIsConst;
    bool bIsReference;
    
    FLocalVariableInfo() : bIsConst(false), bIsReference(false) {}
};
