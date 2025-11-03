#pragma once

#include "CoreMinimal.h"
#include "Core/Result.h"
#include "Core/ServiceBase.h"
#include "Json.h"

// Forward declarations
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node;
struct FEdGraphPinType;

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
};
