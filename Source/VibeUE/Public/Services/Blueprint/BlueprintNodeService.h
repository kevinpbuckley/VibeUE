#pragma once

#include "CoreMinimal.h"
#include "Core/Result.h"
#include "Services/Common/ServiceBase.h"

// Forward declarations
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node;
struct FEdGraphPinType;
struct FGraphInfo;

/**
 * Information about a pin connection
 */
struct VIBEUE_API FPinConnectionInfo
{
    FString SourceNodeId;
    FString SourcePinName;
    FString TargetNodeId;
    FString TargetPinName;
    FString PinType;
};

struct VIBEUE_API FPinLinkBreakInfo
{
    FString OtherNodeId;
    FString OtherNodeClass;
    FString OtherPinId;
    FString OtherPinName;
    FString PinRole;
};

struct VIBEUE_API FPinLinkCreateInfo
{
    FString FromPinId;
    FString ToPinId;
    FString FromPinRole;
    FString ToNodeId;
    FString ToNodeClass;
    FString ToPinName;
};

struct VIBEUE_API FPinConnectionRequest
{
    int32 Index = 0;
    UEdGraphPin* SourcePin = nullptr;
    UEdGraphNode* SourceNode = nullptr;
    UEdGraph* SourceGraph = nullptr;
    FString SourceNodeId;
    FString SourcePinIdentifier;
    UEdGraphPin* TargetPin = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    UEdGraph* TargetGraph = nullptr;
    FString TargetNodeId;
    FString TargetPinIdentifier;
    bool bAllowConversionNode = true;
    bool bAllowPromotion = true;
    bool bBreakExistingLinks = true;
};

struct VIBEUE_API FPinConnectionResult
{
    int32 Index = 0;
    bool bSuccess = false;
    bool bAlreadyConnected = false;
    FString ErrorCode;
    FString ErrorMessage;
    FString SchemaResponse;
    FString SourceNodeId;
    FString TargetNodeId;
    FString SourcePinIdentifier;
    FString TargetPinIdentifier;
    TArray<FPinLinkBreakInfo> BrokenLinks;
    TArray<FPinLinkCreateInfo> CreatedLinks;
    bool bGraphModified = false;
    UEdGraph* ModifiedGraph = nullptr;
    bool bBlueprintModified = false;
};

struct VIBEUE_API FPinConnectionBatchResult
{
    TArray<FPinConnectionResult> Results;
    TArray<UEdGraph*> ModifiedGraphs;
    bool bBlueprintModified = false;
};

/**
 * Request to disconnect pins
 */
struct VIBEUE_API FPinDisconnectionRequest
{
    int32 Index = 0;
    UEdGraphPin* SourcePin = nullptr;
    UEdGraphNode* SourceNode = nullptr;
    UEdGraph* SourceGraph = nullptr;
    FString SourcePinIdentifier;
    UEdGraphPin* TargetPin = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    UEdGraph* TargetGraph = nullptr;
    FString TargetPinIdentifier;
    bool bBreakAll = true;  // If true, break all links; if false, break specific link
};

/**
 * Result of pin disconnection operation
 */
struct VIBEUE_API FPinDisconnectionResult
{
    int32 Index = 0;
    bool bSuccess = false;
    FString ErrorCode;
    FString ErrorMessage;
    FString PinIdentifier;
    FString SourcePinIdentifier;
    FString TargetPinIdentifier;
    TArray<FPinLinkBreakInfo> BrokenLinks;
    bool bGraphModified = false;
    UEdGraph* ModifiedGraph = nullptr;
};

/**
 * Batch disconnection results
 */
struct VIBEUE_API FPinDisconnectionBatchResult
{
    TArray<FPinDisconnectionResult> Results;
    TArray<UEdGraph*> ModifiedGraphs;
    bool bBlueprintModified = false;
};

/**
 * Descriptor for a node type available in the palette
 */
struct VIBEUE_API FNodeDescriptor
{
    FString NodeType;
    FString DisplayName;
    FString Category;
    FString Description;
    TArray<FString> Keywords;
    FString SpawnerKey;
};

/**
 * Detailed information about a specific node instance
 */
struct VIBEUE_API FNodeInfo
{
    FString NodeId;
    FString NodeType;
    FString DisplayName;
    FVector2D Position;
    FString GraphName;
    TArray<FPinConnectionInfo> Connections;
    TMap<FString, FString> Properties;
};

/**
 * Information about a deleted node
 */
struct VIBEUE_API FNodeDeletionInfo
{
    FString NodeId;
    FString NodeType;
    FString GraphName;
    TArray<FPinConnectionInfo> DisconnectedPins;
    bool bWasProtected;
};

/**
 * Parameters for creating an input action node
 */
struct VIBEUE_API FInputActionNodeParams
{
    FString ActionName;
    FVector2D Position;
};

/**
 * Configuration for creating an event node
 */
struct VIBEUE_API FEventConfiguration
{
    FString EventName;
    FVector2D Position;
    FString GraphName; // Optional: target graph name (defaults to EventGraph when empty)
};

/**
 * Service for Blueprint node operations (create, connect, configure)
 * Extracted from BlueprintNodeCommands.cpp to provide focused node management
 */
class VIBEUE_API FBlueprintNodeService : public FServiceBase
{
public:
    explicit FBlueprintNodeService(TSharedPtr<FServiceContext> Context);
    virtual ~FBlueprintNodeService() = default;
    
    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("BlueprintNodeService"); }
    
    // Node lifecycle
    TResult<UK2Node*> CreateNode(UBlueprint* Blueprint, const FString& GraphName, 
                                 const FString& NodeType, const FVector2D& Position);
    TResult<FNodeDeletionInfo> DeleteNode(UBlueprint* Blueprint, const FString& NodeId, bool bDisconnectPins = true);
    TResult<void> MoveNode(UBlueprint* Blueprint, const FString& NodeId, const FVector2D& Position);
    
    // Specialized node creation
    TResult<FString> CreateInputActionNode(UBlueprint* Blueprint, const FInputActionNodeParams& Params);
    TResult<FString> AddEvent(UBlueprint* Blueprint, const FEventConfiguration& Config);
    
    // Pin connections
    TResult<FPinConnectionResult> ConnectPins(UBlueprint* Blueprint, const FPinConnectionRequest& Request);
    TResult<FPinConnectionBatchResult> ConnectPinsBatch(UBlueprint* Blueprint, const TArray<FPinConnectionRequest>& Requests);
    
    // Pin disconnections
    TResult<FPinDisconnectionResult> DisconnectPins(UBlueprint* Blueprint, const FPinDisconnectionRequest& Request);
    TResult<FPinDisconnectionBatchResult> DisconnectPinsBatch(UBlueprint* Blueprint, const TArray<FPinDisconnectionRequest>& Requests);
    TResult<TArray<FPinConnectionInfo>> GetPinConnections(UBlueprint* Blueprint, const FString& NodeId);
    
    // Node configuration
    TResult<void> SetPinDefaultValue(UBlueprint* Blueprint, const FString& NodeId,
                                     const FString& PinName, const FString& Value);
    TResult<FString> GetPinDefaultValue(UBlueprint* Blueprint, const FString& NodeId,
                                       const FString& PinName);
    TResult<void> ConfigureNode(UBlueprint* Blueprint, const FString& NodeId,
                               const TMap<FString, FString>& Config);
    
    // Node discovery
    TResult<TArray<FNodeDescriptor>> DiscoverAvailableNodes(UBlueprint* Blueprint, 
                                                           const FString& SearchTerm);
    TResult<FNodeInfo> GetNodeDetails(UBlueprint* Blueprint, const FString& NodeId);
    TResult<TArray<FString>> ListNodes(UBlueprint* Blueprint, const FString& GraphName);
    TResult<TArray<FNodeInfo>> FindNodes(UBlueprint* Blueprint, const FString& GraphName,
                                        const FString& SearchTerm);
    
    // Node refresh/reconstruction
    TResult<void> RefreshNode(UBlueprint* Blueprint, const FString& NodeId, bool bCompile = true);
    TResult<TArray<FGraphInfo>> RefreshAllNodes(UBlueprint* Blueprint, bool bCompile = true);

private:
    /**
     * Helper method to resolve target graph by name
     * @param Blueprint Target blueprint
     * @param GraphName Graph name or scope (empty defaults to event graph)
     * @param OutError Error message if resolution fails
     * @return Resolved graph or nullptr on failure
     */
    UEdGraph* ResolveTargetGraph(UBlueprint* Blueprint, const FString& GraphName, FString& OutError);

    /**
     * Helper method to gather all candidate graphs from a blueprint
     * @param Blueprint Target blueprint
     * @param PreferredGraph Optional preferred graph to add first
     * @param OutGraphs Output array of graphs
     */
    void GatherCandidateGraphs(UBlueprint* Blueprint, UEdGraph* PreferredGraph, TArray<UEdGraph*>& OutGraphs);

    /**
     * Helper method to resolve a node by identifier (GUID, name, etc.)
     * @param Identifier Node identifier string
     * @param Graphs Graphs to search in
     * @param OutNode Output node if found
     * @param OutGraph Output graph containing the node
     * @return True if node was found
     */
    bool ResolveNodeIdentifier(const FString& Identifier, const TArray<UEdGraph*>& Graphs, 
                               UEdGraphNode*& OutNode, UEdGraph*& OutGraph);

    /**
     * Helper method to find a pin on a node by name
     * @param Node Node to search
     * @param PinName Pin name to find
     * @return Found pin or nullptr
     */
    UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName);
};
