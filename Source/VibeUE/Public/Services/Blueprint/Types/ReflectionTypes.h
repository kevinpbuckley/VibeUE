#pragma once

#include "CoreMinimal.h"

/**
 * Blueprint Reflection Type Definitions
 * 
 * This header contains data structures related to Blueprint type reflection,
 * node discovery, and type cataloging.
 */

/**
 * Search criteria for available node types
 */
struct VIBEUE_API FNodeTypeSearchCriteria
{
    TOptional<FString> Category;
    TOptional<FString> SearchTerm;
    TOptional<FString> ClassFilter;
    bool bIncludeFunctions = true;
    bool bIncludeVariables = true;
    bool bIncludeEvents = true;
    bool bReturnDescriptors = true;
    int32 MaxResults = 100;
    
    FNodeTypeSearchCriteria() = default;
};

/**
 * Information about an available node type
 */
struct VIBEUE_API FNodeTypeInfo
{
    FString SpawnerKey;
    FString NodeTitle;
    FString Category;
    FString NodeType;
    FString Description;
    FString Keywords;
    int32 ExpectedPinCount = 0;
    bool bIsStatic = false;
    
    FNodeTypeInfo() = default;
};

/**
 * Result structure for input key discovery
 */
struct VIBEUE_API FInputKeyResult
{
	TArray<TSharedPtr<class FJsonObject>> Keys;
	int32 TotalCount;
	int32 KeyboardCount;
	int32 MouseCount;
	int32 GamepadCount;
	int32 OtherCount;
	FString Category;
	
	FInputKeyResult()
		: TotalCount(0)
		, KeyboardCount(0)
		, MouseCount(0)
		, GamepadCount(0)
		, OtherCount(0)
		, Category(TEXT("All"))
	{
	}
};
