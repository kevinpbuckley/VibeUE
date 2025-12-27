// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Blueprint Node Type Definitions
 * 
 * This header contains data structures related to Blueprint nodes,
 * pins, connections, and node operations.
 */

/**
 * @struct FPinConnectionRequest
 * @brief Structure for requesting a pin connection
 * 
 * Contains all parameters needed to establish a connection between two pins,
 * including options for conversion nodes and automatic type promotion.
 */
struct VIBEUE_API FPinConnectionRequest
{
	/** Source pin unique identifier */
	FString SourcePinId;
	
	/** Target pin unique identifier */
	FString TargetPinId;
	
	/** Source node unique identifier */
	FString SourceNodeId;
	
	/** Target node unique identifier */
	FString TargetNodeId;
	
	/** Source pin name */
	FString SourcePinName;
	
	/** Target pin name */
	FString TargetPinName;
	
	/** Allow automatic conversion nodes */
	bool bAllowConversion = true;
	
	/** Allow automatic type promotion */
	bool bAllowPromotion = true;
	
	/** Break existing connections */
	bool bBreakExisting = true;
	
	FPinConnectionRequest() = default;
};

/**
 * @struct FPinLinkBreakInfo
 * @brief Information about a broken pin link
 * 
 * Describes a connection that was broken during a pin operation.
 */
struct VIBEUE_API FPinLinkBreakInfo
{
	/** Node identifier containing the pin */
	FString NodeId;
	
	/** Name of the pin that was disconnected */
	FString PinName;
	
	/** Identifier of the other node in the connection */
	FString OtherNodeId;
	
	/** Name of the pin on the other node */
	FString OtherPinName;
	
	FPinLinkBreakInfo() = default;
};

/**
 * @struct FPinLinkCreateInfo
 * @brief Information about a created pin link
 * 
 * Describes a new connection that was created during a pin operation.
 */
struct VIBEUE_API FPinLinkCreateInfo
{
	/** Source node unique identifier */
	FString SourceNodeId;
	
	/** Source pin name */
	FString SourcePinName;
	
	/** Target node unique identifier */
	FString TargetNodeId;
	
	/** Target pin name */
	FString TargetPinName;
	
	FPinLinkCreateInfo() = default;
};

/**
 * @struct FPinConnectionResult
 * @brief Result of a pin connection operation
 * 
 * Contains success status, lists of broken and created links,
 * and error information if the operation failed.
 */
struct VIBEUE_API FPinConnectionResult
{
	/** Whether the connection operation succeeded */
	bool bSuccess = false;
	
	/** Links that were broken during the operation */
	TArray<FPinLinkBreakInfo> BrokenLinks;
	
	/** Links that were created during the operation */
	TArray<FPinLinkCreateInfo> CreatedLinks;
	
	/** Error message if operation failed */
	FString ErrorMessage;
	
	/** Error code if operation failed */
	FString ErrorCode;
	
	FPinConnectionResult() = default;
};

/**
 * @struct FPinDisconnectionResult
 * @brief Result of a pin disconnection operation
 * 
 * Contains success status, count of disconnected links,
 * and details about which connections were broken.
 */
struct VIBEUE_API FPinDisconnectionResult
{
	/** Whether the disconnection operation succeeded */
	bool bSuccess = false;
	
	/** Number of connections that were broken */
	int32 DisconnectedCount = 0;
	
	/** Details of the disconnected links */
	TArray<FPinLinkBreakInfo> DisconnectedLinks;
	
	/** Error message if operation failed */
	FString ErrorMessage;
	
	/** Error code if operation failed */
	FString ErrorCode;
	
	FPinDisconnectionResult() = default;
};

/**
 * @struct FNodeCreationParams
 * @brief Parameters for creating a Blueprint node
 * 
 * Contains all parameters needed to create a node in a Blueprint graph,
 * including spawner key, position, and additional configuration.
 */
struct VIBEUE_API FNodeCreationParams
{
	/** Spawner key for exact node type creation */
	FString SpawnerKey;
	
	/** Node type name (alternative to spawner key) */
	FString NodeType;
	
	/** Target graph name (empty for event graph) */
	FString GraphName;
	
	/** X position for node placement */
	int32 PosX = 0;
	
	/** Y position for node placement */
	int32 PosY = 0;
	
	/** Additional parameters for node creation */
	TArray<TSharedPtr<FJsonObject>> AdditionalParams;
	
	FNodeCreationParams() = default;
};

/**
 * @struct FNodeInfo
 * @brief Detailed information about a Blueprint node
 * 
 * Contains comprehensive metadata about a node including its ID,
 * type, position, pins, and additional properties.
 */
struct VIBEUE_API FNodeInfo
{
	/** Unique node identifier (GUID) */
	FString NodeId;
	
	/** Node type name */
	FString NodeType;
	
	/** Node class name */
	FString NodeClass;
	
	/** Node display title */
	FString Title;
	
	/** Graph containing the node */
	FString GraphName;
	
	/** X position in graph */
	int32 PosX = 0;
	
	/** Y position in graph */
	int32 PosY = 0;
	
	/** Node pin information */
	TArray<TSharedPtr<FJsonObject>> Pins;
	
	/** Additional node metadata */
	TSharedPtr<FJsonObject> AdditionalInfo;
	
	FNodeInfo() = default;
};
