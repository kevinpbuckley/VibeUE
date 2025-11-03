#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Core/Result.h"

// Forward declare reflection commands
class FBlueprintReflectionCommands;

/**
 * Handler class for Blueprint Node-related MCP commands
 * Enhanced with reflection-based node discovery and manipulation
 */
class VIBEUE_API FBlueprintNodeCommands
{
public:
    FBlueprintNodeCommands();

    // Handle blueprint node commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Existing blueprint node command handlers
    TSharedPtr<FJsonObject> HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintEvent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintInputActionNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListEventGraphNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetNodeDetails(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDescribeBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectPins(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDisconnectPins(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListCustomEvents(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRefreshBlueprintNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRefreshBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    // New unified function management (list/get/create/delete) Phase 1
    TSharedPtr<FJsonObject> HandleManageBlueprintFunction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleManageBlueprintNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleMoveBlueprintNode(const TSharedPtr<FJsonObject>& Params);
    
    // NEW: Reflection-based command handlers
    TSharedPtr<FJsonObject> HandleGetAvailableBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDiscoverNodesWithDescriptors(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleResetPinDefaults(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConfigureBlueprintNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSplitOrRecombinePins(const TSharedPtr<FJsonObject>& Params, bool bSplitPins);
    
    // NEW: Deletion command handlers
    TSharedPtr<FJsonObject> HandleDeleteBlueprintNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteBlueprintEventNode(const TSharedPtr<FJsonObject>& Params);

    // NEW (Oct 6, 2025): Component Event Support - Reflection-Based
    TSharedPtr<FJsonObject> HandleCreateComponentEvent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetComponentEvents(const TSharedPtr<FJsonObject>& Params);
    
    // NEW (Oct 6, 2025): Input Key Discovery - Reflection-Based
    TSharedPtr<FJsonObject> HandleGetAllInputKeys(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateInputKeyNode(const TSharedPtr<FJsonObject>& Params);

private:
    // Internal helpers (Phase 1 minimal)
    TSharedPtr<FJsonObject> BuildFunctionSummary(UBlueprint* Blueprint);
    TSharedPtr<FJsonObject> BuildSingleFunctionInfo(UBlueprint* Blueprint, const FString& FunctionName);
    bool FindUserFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName, UEdGraph*& OutGraph) const;
    TSharedPtr<FJsonObject> CreateFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName);
    bool RemoveFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName, FString& OutError);

    // Function parameter & property helpers (simplified initial implementation)
    TArray<TSharedPtr<FJsonValue>> ListFunctionParameters(UBlueprint* Blueprint, UEdGraph* FunctionGraph) const;
    TSharedPtr<FJsonObject> AddFunctionParameter(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& ParamName, const FString& TypeDesc, const FString& Direction);
    TSharedPtr<FJsonObject> RemoveFunctionParameter(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& ParamName, const FString& Direction);
    TSharedPtr<FJsonObject> UpdateFunctionParameter(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& ParamName, const FString& Direction, const FString& NewType, const FString& NewName);
    TSharedPtr<FJsonObject> UpdateFunctionProperties(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const TSharedPtr<FJsonObject>& Params);
    // Local variable helpers
    TArray<TSharedPtr<FJsonValue>> ListFunctionLocalVariables(UBlueprint* Blueprint, UEdGraph* FunctionGraph) const;
    TSharedPtr<FJsonObject> AddFunctionLocalVariable(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& VarName, const FString& TypeDesc, const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> RemoveFunctionLocalVariable(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& VarName);
    TSharedPtr<FJsonObject> UpdateFunctionLocalVariable(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& VarName, const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> BuildAvailableLocalVariableTypes() const;
    bool ParseTypeDescriptor(const FString& TypeDesc, FEdGraphPinType& OutType, FString& OutError) const;
    FString DescribePinType(const FEdGraphPinType& PinType) const;
    const UStruct* ResolveFunctionScopeStruct(UBlueprint* Blueprint, UEdGraph* FunctionGraph) const;
    class UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* FunctionGraph) const;

    // Graph scoping helper for node operations
    UEdGraph* ResolveTargetGraph(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params, FString& OutError) const;
    void GatherCandidateGraphs(UBlueprint* Blueprint, UEdGraph* PreferredGraph, TArray<UEdGraph*>& OutGraphs) const;
    bool ResolveNodeIdentifier(const FString& Identifier, const TArray<UEdGraph*>& Graphs, UEdGraphNode*& OutNode, UEdGraph*& OutGraph) const;
    FString DescribeAvailableNodes(const TArray<UEdGraph*>& Graphs) const;

    struct FResolvedPinReference
    {
        UEdGraphPin* Pin = nullptr;
        UEdGraphNode* Node = nullptr;
        UEdGraph* Graph = nullptr;
        FString Identifier;
    };

    bool ResolvePinByIdentifier(const TArray<UEdGraph*>& Graphs, const FString& Identifier, FResolvedPinReference& OutPin) const;
    bool ResolvePinByNodeAndName(const TArray<UEdGraph*>& Graphs, const FString& NodeIdentifier, const FString& PinName, EEdGraphPinDirection DesiredDirection, FResolvedPinReference& OutPin, FString& OutError) const;
    bool ResolvePinFromPayload(const TSharedPtr<FJsonObject>& Payload, const TArray<FString>& RolePrefixes, EEdGraphPinDirection DesiredDirection, const TArray<UEdGraph*>& Graphs, FResolvedPinReference& OutPin, FString& OutError) const;

    bool ResolveNodeContext(const TSharedPtr<FJsonObject>& Params,
        UBlueprint*& OutBlueprint,
        UEdGraphNode*& OutNode,
        UEdGraph*& OutGraph,
        TArray<UEdGraph*>& OutCandidateGraphs,
        FString& OutBlueprintName,
        FString& OutNodeIdentifier,
        FString& OutError) const;

    TSharedPtr<FJsonObject> ApplyPinTransform(
        UBlueprint* Blueprint,
        UEdGraphNode* Node,
        const FString& BlueprintName,
        const FString& NodeIdentifier,
        const TArray<FString>& PinNames,
        bool bSplitPins) const;

    // HandleConnectPins helper methods
    struct FConnectionDefaults
    {
        bool bAllowConversion = true;
        bool bAllowPromotion = true;
        bool bBreakExisting = true;
    };
    
    FConnectionDefaults ParseConnectionDefaults(const TSharedPtr<FJsonObject>& Params) const;
    TArray<TSharedPtr<FJsonValue>> ConvertBrokenLinksToJson(const TArray<struct FPinLinkBreakInfo>& Links) const;
    TArray<TSharedPtr<FJsonValue>> ConvertCreatedLinksToJson(const TArray<struct FPinLinkCreateInfo>& Links) const;
    TSharedPtr<FJsonObject> BuildConnectionSuccessJson(const struct FPinConnectionResult& Result, 
                                                        const TSharedPtr<FJsonObject>& RequestObject) const;
    TSharedPtr<FJsonObject> BuildConnectionResponseJson(const FString& BlueprintName,
                                                         int32 AttemptedCount,
                                                         const TArray<TSharedPtr<FJsonValue>>& Successes,
                                                         const TArray<TSharedPtr<FJsonValue>>& Failures,
                                                         const TArray<UEdGraph*>& ModifiedGraphs) const;
    
    // HandleDisconnectPins helper methods
    TArray<TSharedPtr<FJsonValue>> SummarizeLinksToJson(UEdGraphPin* Pin, const FString& Role) const;
    TSharedPtr<FJsonObject> BuildDisconnectionSuccessJson(const struct FPinDisconnectionResult& Result,
                                                            const TSharedPtr<FJsonObject>& RequestObject) const;
    TSharedPtr<FJsonObject> BuildDisconnectionResponseJson(const FString& BlueprintName,
                                                             int32 AttemptedCount,
                                                             const TArray<TSharedPtr<FJsonValue>>& Successes,
                                                             const TArray<TSharedPtr<FJsonValue>>& Failures,
                                                             const TArray<UEdGraph*>& ModifiedGraphs) const;
    
private:
    // Helper methods to convert TResult to JSON
    TSharedPtr<FJsonObject> CreateSuccessResponse() const;
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorCode, const FString& ErrorMessage) const;
    template<typename T>
    TSharedPtr<FJsonObject> ConvertTResultToJson(const TResult<T>& Result) const;
    
    // Reflection system helper
    TSharedPtr<FBlueprintReflectionCommands> ReflectionCommands;
    
    // Phase 4: Blueprint Services (replacing inline logic)
    TSharedPtr<class FBlueprintDiscoveryService> DiscoveryService;
    TSharedPtr<class FBlueprintLifecycleService> LifecycleService;
    TSharedPtr<class FBlueprintPropertyService> PropertyService;
    TSharedPtr<class FBlueprintComponentService> ComponentService;
    TSharedPtr<class FBlueprintFunctionService> FunctionService;
    TSharedPtr<class FBlueprintNodeService> NodeService;
    TSharedPtr<class FBlueprintGraphService> GraphService;
    TSharedPtr<class FBlueprintReflectionService> ReflectionService;
}; 
