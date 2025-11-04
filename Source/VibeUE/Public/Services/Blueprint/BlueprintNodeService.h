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
 * Function metadata for node descriptors
 */
struct VIBEUE_API FFunctionMetadata
{
	FString FunctionName;
	FString FunctionClassName;
	FString FunctionClassPath;
	bool bIsStatic;
	bool bIsConst;
	bool bIsPure;
	FString Module;
	
	FFunctionMetadata()
		: bIsStatic(false)
		, bIsConst(false)
		, bIsPure(false)
	{
	}
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
 * Detailed information about a pin (used by DescribeNode from PR #115)
 */
struct VIBEUE_API FPinInfo
{
    FString PinId;
    FString Name;
    FString Direction;
    FString Category;
    FString SubCategory;
    FString PinTypePath;
    FString Type;  // Simple type name (for node descriptors)
    FString TypePath;  // Full type path (for node descriptors)
    FString Container;
    bool bIsConst = false;
    bool bIsReference = false;
    bool bIsArray = false;
    bool bIsSet = false;
    bool bIsMap = false;
    bool bIsHidden = false;
    bool bIsAdvanced = false;
    bool bIsConnected = false;
    FString Tooltip;
    FString DefaultValue;
    FString DefaultText;
    FString DefaultObjectPath;
    TSharedPtr<FJsonValue> DefaultValueJson;
    TArray<TSharedPtr<FJsonObject>> Links;
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
    
    // Extended fields for detailed node information
    FString NodeTitle;
    FString NodeClassName;
    FString NodeClassPath;
    FString Tooltip;
    int32 ExpectedPinCount = 0;
    bool bIsStatic = false;
    TArray<FPinInfo> Pins;
    TOptional<FFunctionMetadata> FunctionMetadata;
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
 * Detailed information about a blueprint node with pins (used by DescribeNode from PR #115)
 */
struct VIBEUE_API FDetailedNodeInfo
{
    FString NodeId;
    FString DisplayName;
    FString ClassPath;
    FString GraphScope;
    FString GraphName;
    FString GraphGuid;
    FVector2D Position;
    FString Comment;
    bool bIsPure = false;
    FString ExecState;
    TSharedPtr<FJsonObject> NodeDescriptor;
    FString SpawnerKey;
    TSharedPtr<FJsonObject> NodeParams;
    TSharedPtr<FJsonObject> FunctionMetadata;
    TSharedPtr<FJsonObject> VariableMetadata;
    TSharedPtr<FJsonObject> CastMetadata;
    TArray<FPinInfo> Pins;
    TSharedPtr<FJsonObject> Metadata;
};

/**
 * Pin detail information (used by GetNodeDetails from PR #118)
 */
struct VIBEUE_API FPinDetail
{
    FString PinName;
    FString PinType;
    FString Direction;  // "Input" or "Output"
    bool bIsHidden;
    bool bIsConnected;
    FString DefaultValue;
    FString DefaultObjectName;
    FString DefaultTextValue;
    bool bIsArray;
    bool bIsReference;
    TArray<FPinConnectionInfo> Connections;
};

/**
 * Comprehensive node details with pin information (used by GetNodeDetails from PR #118)
 */
struct VIBEUE_API FNodeDetails
{
    FString NodeId;
    FString NodeType;
    FString DisplayName;
    FVector2D Position;
    FString GraphName;
    bool bCanUserDeleteNode;
    FString Category;
    FString Tooltip;
    FString Keywords;
    TArray<FPinDetail> InputPins;
    TArray<FPinDetail> OutputPins;
    TMap<FString, FString> Properties;
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
    FString GraphName; // Optional: target graph name (defaults to EventGraph if empty or not specified)
};

/**
 * Parameters for creating a node with spawner key
 */
struct VIBEUE_API FNodeCreationParams
{
    FString SpawnerKey;      // Required: spawner key from descriptor discovery
    FVector2D Position;      // Node position
    FString GraphScope;      // "event" or "function"
    FString FunctionName;    // Required if GraphScope is "function"
    TSharedPtr<FJsonObject> NodeParams; // Optional: additional node configuration
    
    FNodeCreationParams()
        : Position(500.0f, 500.0f)
        , GraphScope(TEXT("event"))
    {
    }
};

/**
 * Search criteria for finding nodes in a blueprint
 */
struct VIBEUE_API FNodeSearchCriteria
{
    FString NodeType;        // Node type to search for (e.g., "K2Node_Event")
    FString NamePattern;     // Optional: Name pattern to match
    FString GraphScope;      // Optional: Graph name to search in (empty = all graphs)
    TOptional<FString> FunctionName;  // Reserved for future use: Function-scoped search
};

/**
 * Node property information
 */
struct VIBEUE_API FNodePropertyInfo
{
    FString PropertyName;
    FString CurrentValue;
    FString PropertyType;
    FString Category;
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
    TResult<FString> CreateNodeFromSpawnerKey(UBlueprint* Blueprint, const FNodeCreationParams& Params);
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
    TResult<void> SetNodeProperty(UBlueprint* Blueprint, const FString& NodeId,
                                  const FString& PropertyName, const FString& PropertyValue);
    TResult<FNodePropertyInfo> GetNodeProperty(UBlueprint* Blueprint, const FString& NodeId,
                                              const FString& PropertyName);
    TResult<FString> GetPinDefaultValue(UBlueprint* Blueprint, const FString& NodeId,
                                       const FString& PinName);
    TResult<void> ConfigureNode(UBlueprint* Blueprint, const FString& NodeId,
                               const TMap<FString, FString>& Config);
    
    // Pin manipulation
    TResult<void> SplitPin(UBlueprint* Blueprint, const FString& NodeId, const FString& PinName);
    TResult<void> RecombinePin(UBlueprint* Blueprint, const FString& NodeId, const FString& PinName);
    TResult<void> ResetPinToDefault(UBlueprint* Blueprint, const FString& NodeId, const FString& PinName);
    
    // Node discovery
    TResult<TArray<FNodeDescriptor>> DiscoverAvailableNodes(UBlueprint* Blueprint, 
                                                           const FString& SearchTerm);
    TResult<FNodeInfo> GetNodeDetails(UBlueprint* Blueprint, const FString& NodeId);
    TResult<FNodeDetails> GetNodeDetailsExtended(UBlueprint* Blueprint, const FString& NodeId, 
                                                 bool bIncludePins = true, bool bIncludeConnections = true);
    TResult<TArray<FString>> ListNodes(UBlueprint* Blueprint, const FString& GraphName);
    TResult<TArray<FNodeInfo>> FindNodes(UBlueprint* Blueprint, const FString& GraphName,
                                        const FString& SearchTerm);
    TResult<TArray<FNodeInfo>> FindNodes(UBlueprint* Blueprint, const FNodeSearchCriteria& Criteria);
    
    // Node refresh/reconstruction
    TResult<void> RefreshNode(UBlueprint* Blueprint, const FString& NodeId, bool bCompile = true);
    TResult<TArray<FGraphInfo>> RefreshAllNodes(UBlueprint* Blueprint, bool bCompile = true);
    
    // Node description (detailed information with pins)
    TResult<FDetailedNodeInfo> DescribeNode(UBlueprint* Blueprint, const FString& NodeId, bool bIncludePins = true, bool bIncludeInternalPins = false);
    TResult<TArray<FDetailedNodeInfo>> DescribeAllNodes(UBlueprint* Blueprint, const FString& GraphScope, 
                                                        bool bIncludePins = true, bool bIncludeInternalPins = false,
                                                        int32 Offset = 0, int32 Limit = -1);
    
    // Helper to convert detailed node info to JSON
    static TSharedPtr<FJsonObject> ConvertNodeInfoToJson(const FDetailedNodeInfo& NodeInfo, bool bIncludePins = true);
    static TSharedPtr<FJsonObject> ConvertPinInfoToJson(const FPinInfo& PinInfo);

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
