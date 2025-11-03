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
    TResult<void> DeleteNode(UBlueprint* Blueprint, const FString& NodeId);
    TResult<void> MoveNode(UBlueprint* Blueprint, const FString& NodeId, const FVector2D& Position);
    
    // Pin connections
    TResult<void> ConnectPins(UBlueprint* Blueprint, const FString& SourceNodeId, 
                             const FString& SourcePinName, const FString& TargetNodeId, 
                             const FString& TargetPinName);
    TResult<void> DisconnectPins(UBlueprint* Blueprint, const FString& SourceNodeId,
                                const FString& SourcePinName);
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
