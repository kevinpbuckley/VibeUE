#pragma once

#include "CoreMinimal.h"

/**
 * Blueprint Graph Type Definitions
 * 
 * This header contains data structures related to Blueprint graphs
 * and nodes.
 */

/**
 * Information about a graph
 */
struct VIBEUE_API FGraphInfo
{
    FString Name;
    FString Guid;
    FString GraphType; // "event", "function", "macro", etc.
    int32 NodeCount;
    
    FGraphInfo()
        : NodeCount(0)
    {}
};

/**
 * Summary information about a node in a graph
 */
struct VIBEUE_API FNodeSummary
{
    FString NodeId;
    FString NodeType;
    FString Title;
    TArray<TSharedPtr<class FJsonObject>> Pins;
    
    FNodeSummary()
    {}
};
