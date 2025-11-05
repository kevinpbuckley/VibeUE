#pragma once

#include "CoreMinimal.h"
#include "Core/Result.h"
#include "Services/Common/ServiceBase.h"
#include "Services/Blueprint/Types/GraphTypes.h"
#include "Services/Blueprint/Types/ReflectionTypes.h"
#include "EdGraph/EdGraphPin.h"

// Forward declarations
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class FJsonObject;

/**
 * Pin connection request structure
 */
struct VIBEUE_API FPinConnectionRequest
{
	FString SourcePinId;
	FString TargetPinId;
	FString SourceNodeId;
	FString TargetNodeId;
	FString SourcePinName;
	FString TargetPinName;
	bool bAllowConversion = true;
	bool bAllowPromotion = true;
	bool bBreakExisting = true;
	
	FPinConnectionRequest() = default;
};

/**
 * Information about a broken pin link
 */
struct VIBEUE_API FPinLinkBreakInfo
{
	FString NodeId;
	FString PinName;
	FString OtherNodeId;
	FString OtherPinName;
	
	FPinLinkBreakInfo() = default;
};

/**
 * Information about a created pin link
 */
struct VIBEUE_API FPinLinkCreateInfo
{
	FString SourceNodeId;
	FString SourcePinName;
	FString TargetNodeId;
	FString TargetPinName;
	
	FPinLinkCreateInfo() = default;
};

/**
 * Result of a pin connection operation
 */
struct VIBEUE_API FPinConnectionResult
{
	bool bSuccess = false;
	TArray<FPinLinkBreakInfo> BrokenLinks;
	TArray<FPinLinkCreateInfo> CreatedLinks;
	FString ErrorMessage;
	FString ErrorCode;
	
	FPinConnectionResult() = default;
};

/**
 * Result of a pin disconnection operation
 */
struct VIBEUE_API FPinDisconnectionResult
{
	bool bSuccess = false;
	int32 DisconnectedCount = 0;
	TArray<FPinLinkBreakInfo> DisconnectedLinks;
	FString ErrorMessage;
	FString ErrorCode;
	
	FPinDisconnectionResult() = default;
};

/**
 * Node creation parameters
 */
struct VIBEUE_API FNodeCreationParams
{
	FString SpawnerKey;
	FString NodeType;
	FString GraphName;
	int32 PosX = 0;
	int32 PosY = 0;
	TArray<TSharedPtr<FJsonObject>> AdditionalParams;
	
	FNodeCreationParams() = default;
};

/**
 * Node information (detailed)
 */
struct VIBEUE_API FNodeInfo
{
	FString NodeId;
	FString NodeType;
	FString NodeClass;
	FString Title;
	FString GraphName;
	int32 PosX = 0;
	int32 PosY = 0;
	TArray<TSharedPtr<FJsonObject>> Pins;
	TSharedPtr<FJsonObject> AdditionalInfo;
	
	FNodeInfo() = default;
};

/**
 * Service for Blueprint node manipulation
 * Handles node creation, discovery, pin operations, and configuration
 */
class VIBEUE_API FBlueprintNodeService : public FServiceBase
{
public:
	explicit FBlueprintNodeService(TSharedPtr<FServiceContext> Context);
	virtual ~FBlueprintNodeService() = default;
	
	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("BlueprintNodeService"); }
	
	// Node Discovery
	/**
	 * Find nodes of a specific type in a graph
	 * @param Blueprint The blueprint to search
	 * @param NodeType The type of nodes to find
	 * @param GraphName Optional graph name to search in (empty for event graph)
	 * @return Array of node GUIDs
	 */
	TResult<TArray<FString>> FindNodes(UBlueprint* Blueprint, const FString& NodeType, const FString& GraphName = FString());
	
	/**
	 * Get detailed information about a specific node
	 * @param Blueprint The blueprint containing the node
	 * @param NodeId The GUID of the node
	 * @param GraphName Optional graph name to search in
	 * @return Node information
	 */
	TResult<FNodeInfo> GetNodeDetails(UBlueprint* Blueprint, const FString& NodeId, const FString& GraphName = FString());
	
	/**
	 * Get advanced detailed information about a specific node with optional pin details, properties, and connections
	 * @param Blueprint The blueprint containing the node
	 * @param NodeId The GUID of the node to describe
	 * @param Params JSON object with optional parameters (graph_scope, include_pins, include_properties, include_connections)
	 * @return Complete node descriptor with all requested details as JSON object
	 */
	TResult<TSharedPtr<FJsonObject>> GetNodeDetailsAdvanced(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const TSharedPtr<FJsonObject>& Params);
	
	/**
	 * Describe multiple nodes
	 * @param Blueprint The blueprint containing the nodes
	 * @param NodeIds Array of node GUIDs to describe
	 * @param GraphName Optional graph name to search in
	 * @return Array of node summaries
	 */
	TResult<TArray<FNodeSummary>> DescribeNodes(UBlueprint* Blueprint, const TArray<FString>& NodeIds, const FString& GraphName = FString());
	
	/**
	 * Describe nodes with advanced filtering and pagination
	 * @param Blueprint The blueprint containing the nodes
	 * @param Params JSON object containing filter parameters (graph_scope, node_ids, pin_names, offset, limit, etc.)
	 * @param bIncludePins Whether to include pin information
	 * @param bIncludeInternalPins Whether to include hidden/advanced pins
	 * @return Array of fully described nodes as JSON objects
	 */
	TResult<TArray<TSharedPtr<FJsonObject>>> DescribeNodesAdvanced(
		UBlueprint* Blueprint,
		const TSharedPtr<FJsonObject>& Params,
		bool bIncludePins = true,
		bool bIncludeInternalPins = false
	);
	
	/**
	 * List all nodes in a graph
	 * @param Blueprint The blueprint to query
	 * @param GraphName The graph name (empty for event graph)
	 * @return Array of node summaries
	 */
	TResult<TArray<FNodeSummary>> ListNodes(UBlueprint* Blueprint, const FString& GraphName = FString());
	
	/**
	 * Discover available node types matching criteria
	 * @param Criteria Search criteria for filtering node types
	 * @return Array of available node type information
	 */
	TResult<TArray<FNodeTypeInfo>> DiscoverNodeTypes(const FNodeTypeSearchCriteria& Criteria);
	
	// Node Creation
	/**
	 * Create a new node from a spawner key
	 * @param Blueprint The blueprint to add the node to
	 * @param Params Node creation parameters
	 * @return The created node information
	 */
	TResult<FNodeInfo> CreateNode(UBlueprint* Blueprint, const FNodeCreationParams& Params);
	
	/**
	 * Create an event node
	 * @param Blueprint The blueprint to add the event to
	 * @param EventName The name of the event
	 * @param GraphName Optional graph name
	 * @return The created node information
	 */
	TResult<FNodeInfo> CreateEventNode(UBlueprint* Blueprint, const FString& EventName, const FString& GraphName = FString());
	
	/**
	 * Create an input action node
	 * @param Blueprint The blueprint to add the node to
	 * @param ActionName The name of the input action
	 * @param GraphName Optional graph name
	 * @return The created node information
	 */
	TResult<FNodeInfo> CreateInputActionNode(UBlueprint* Blueprint, const FString& ActionName, const FString& GraphName = FString());
	
	// Pin Operations
	/**
	 * Connect two pins
	 * @param Blueprint The blueprint containing the pins
	 * @param Request Connection request parameters
	 * @return Connection result with broken/created links
	 */
	TResult<FPinConnectionResult> ConnectPins(UBlueprint* Blueprint, const FPinConnectionRequest& Request);
	
	/**
	 * Disconnect pins
	 * @param Blueprint The blueprint containing the pin
	 * @param NodeId The node containing the pin
	 * @param PinName The name of the pin to disconnect
	 * @param GraphName Optional graph name
	 * @return Disconnection result
	 */
	TResult<FPinDisconnectionResult> DisconnectPins(UBlueprint* Blueprint, const FString& NodeId, const FString& PinName, const FString& GraphName = FString());
	
	/**
	 * Split a struct pin into its sub-pins
	 * @param Blueprint The blueprint containing the pin
	 * @param NodeId The node containing the pin
	 * @param PinName The name of the pin to split
	 * @return Success or error
	 */
	TResult<void> SplitPin(UBlueprint* Blueprint, const FString& NodeId, const FString& PinName);
	
	/**
	 * Recombine a split struct pin
	 * @param Blueprint The blueprint containing the pin
	 * @param NodeId The node containing the pin
	 * @param PinName The name of the pin to recombine
	 * @return Success or error
	 */
	TResult<void> RecombinePin(UBlueprint* Blueprint, const FString& NodeId, const FString& PinName);
	
	/**
	 * Configure node with batch pin split/recombine operations
	 * @param Blueprint The blueprint containing the node
	 * @param NodeId The node ID to configure
	 * @param Params JSON object with split/recombine pin lists
	 * @return Configuration result with operation details as JSON
	 */
	TResult<TSharedPtr<FJsonObject>> ConfigureNodeAdvanced(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const TSharedPtr<FJsonObject>& Params);
	
	// Node Configuration
	/**
	 * Set a pin default value or node property
	 * @param Blueprint The blueprint containing the node
	 * @param NodeId The node to configure
	 * @param PropertyName The property or pin name
	 * @param Value The new value
	 * @return Success or error
	 */
	TResult<void> SetNodeProperty(UBlueprint* Blueprint, const FString& NodeId, const FString& PropertyName, const FString& Value);
	
	/**
	 * Reset pin defaults to their original values
	 * @param Blueprint The blueprint containing the node
	 * @param NodeId The node to reset
	 * @param PinNames Optional array of specific pins to reset (empty for all)
	 * @return Success or error
	 */
	TResult<void> ResetPinDefaults(UBlueprint* Blueprint, const FString& NodeId, const TArray<FString>& PinNames = TArray<FString>());
	
	/**
	 * Reset pin defaults with detailed per-pin results
	 * @param Blueprint The blueprint containing the node
	 * @param NodeId The node ID to reset pins on
	 * @param Params JSON object with optional pin list, reset_all flag, compile flag
	 * @return Detailed reset results with per-pin status as JSON
	 */
	TResult<TSharedPtr<FJsonObject>> ResetPinDefaultsAdvanced(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const TSharedPtr<FJsonObject>& Params);
	
	// Node Lifecycle
	/**
	 * Delete a node
	 * @param Blueprint The blueprint containing the node
	 * @param NodeId The GUID of the node to delete
	 * @param GraphName Optional graph name
	 * @return Success or error
	 */
	TResult<void> DeleteNode(UBlueprint* Blueprint, const FString& NodeId, const FString& GraphName = FString());
	
	/**
	 * Move a node to a new position
	 * @param Blueprint The blueprint containing the node
	 * @param NodeId The GUID of the node to move
	 * @param PosX New X position
	 * @param PosY New Y position
	 * @return Success or error
	 */
	TResult<void> MoveNode(UBlueprint* Blueprint, const FString& NodeId, int32 PosX, int32 PosY);
	
	/**
	 * Refresh a node (reconstruct)
	 * @param Blueprint The blueprint containing the node
	 * @param NodeId The GUID of the node to refresh
	 * @return Success or error
	 */
	TResult<void> RefreshNode(UBlueprint* Blueprint, const FString& NodeId);
	
	/**
	 * Refresh all nodes in a blueprint
	 * @param Blueprint The blueprint to refresh
	 * @return Success or error
	 */
	TResult<void> RefreshAllNodes(UBlueprint* Blueprint);

	/**
	 * Connect pins (batch operation with JSON parameters)
	 * @param Blueprint The blueprint containing the pins
	 * @param Params JSON parameters containing connection array
	 * @return JSON object with connection results
	 */
	TResult<TSharedPtr<FJsonObject>> ConnectPinsAdvanced(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params);

private:
	// Helper methods for graph resolution
	UEdGraph* ResolveTargetGraph(UBlueprint* Blueprint, const FString& GraphName) const;
	void GatherCandidateGraphs(UBlueprint* Blueprint, UEdGraph* PreferredGraph, TArray<UEdGraph*>& OutGraphs) const;
	UEdGraphNode* FindNodeByGuid(const TArray<UEdGraph*>& Graphs, const FString& NodeGuid) const;
	UEdGraphPin* FindPinByName(UEdGraphNode* Node, const FString& PinName) const;
	
	// Helper methods for node info construction
	FNodeInfo BuildNodeInfo(UBlueprint* Blueprint, UEdGraphNode* Node) const;
	FNodeSummary BuildNodeSummary(UBlueprint* Blueprint, UEdGraphNode* Node) const;
	TSharedPtr<FJsonObject> BuildPinDescriptor(const UEdGraphPin* Pin) const;
	FString DetermineNodeType(UEdGraphNode* Node) const;
	
	// Helper methods for validation
	bool ValidateNodeGuid(const FString& Guid) const;
	bool ValidatePinDirection(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const;
};
