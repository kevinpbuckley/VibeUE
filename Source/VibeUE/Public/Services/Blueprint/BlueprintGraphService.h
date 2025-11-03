#pragma once

#include "CoreMinimal.h"
#include "Core/Result.h"
#include "Core/ServiceBase.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"

// Forward declarations
class UK2Node_CustomEvent;

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
 * Service for Blueprint graph introspection and manipulation
 * Handles graph analysis, summarization, and basic operations
 * Target: ~300 lines max
 */
class VIBEUE_API FBlueprintGraphService : public FServiceBase
{
public:
    explicit FBlueprintGraphService(TSharedPtr<FServiceContext> Context);
    
    // Graph introspection
    TResult<FString> SummarizeEventGraph(UBlueprint* Blueprint, int32 MaxNodes = 200);
    TResult<TArray<FString>> ListCustomEvents(UBlueprint* Blueprint);
    TResult<TArray<FGraphInfo>> GetAllGraphs(UBlueprint* Blueprint);
    TResult<FGraphInfo> GetGraphInfo(UBlueprint* Blueprint, const FString& GraphName);
    
    // Graph manipulation
    TResult<UEdGraph*> GetGraph(UBlueprint* Blueprint, const FString& GraphName);
    TResult<UEdGraph*> GetEventGraph(UBlueprint* Blueprint);
    TResult<void> ClearGraph(UBlueprint* Blueprint, const FString& GraphName);
    
    // Graph validation
    TResult<TArray<FString>> ValidateGraph(UBlueprint* Blueprint, const FString& GraphName);
    TResult<bool> IsGraphValid(UBlueprint* Blueprint, const FString& GraphName);

private:
    // Helper methods
    FString DescribeGraphScope(const UBlueprint* Blueprint, const UEdGraph* Graph) const;
    FString GetNodeTypeString(const UEdGraphNode* Node) const;
    void GatherCustomEvents(UEdGraph* Graph, TArray<UK2Node_CustomEvent*>& OutEvents) const;
};
