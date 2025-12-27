// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMaterialNodeService;

/**
 * Command handler for Material Graph Node operations.
 * Routes commands from the MCP bridge to FMaterialNodeService.
 * 
 * Actions:
 * 
 * Discovery:
 * - discover_types: Discover available material expression types
 * - get_categories: Get expression categories
 * 
 * Expression Lifecycle:
 * - create: Create a new material expression
 * - delete: Delete an expression
 * - move: Move an expression to a new position
 * 
 * Expression Information:
 * - list: List all expressions in a material
 * - get_details: Get detailed expression information
 * - get_pins: Get all pins for an expression
 * 
 * Connections:
 * - connect: Connect two expressions
 * - disconnect: Disconnect an input
 * - connect_to_output: Connect expression to material output property
 * - disconnect_output: Disconnect a material output property
 * - list_connections: List all connections in material
 * 
 * Expression Properties:
 * - get_property: Get expression property value
 * - set_property: Set expression property value
 * - list_properties: List all editable properties
 * 
 * Parameter Operations:
 * - promote_to_parameter: Convert constant to parameter
 * - create_parameter: Create a parameter expression
 * - set_parameter_metadata: Set parameter group/priority
 * 
 * Material Outputs:
 * - get_output_properties: Get available material output properties
 * - get_output_connections: Get current material output connections
 */
class VIBEUE_API FMaterialNodeCommands
{
public:
	FMaterialNodeCommands();
	~FMaterialNodeCommands();

	/**
	 * Handle a material node command.
	 */
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FMaterialNodeService> Service;

	// Discovery actions
	TSharedPtr<FJsonObject> HandleDiscoverTypes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetCategories(const TSharedPtr<FJsonObject>& Params);

	// Expression lifecycle actions
	TSharedPtr<FJsonObject> HandleCreate(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDelete(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMove(const TSharedPtr<FJsonObject>& Params);

	// Expression information actions
	TSharedPtr<FJsonObject> HandleList(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetDetails(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetPins(const TSharedPtr<FJsonObject>& Params);

	// Connection actions
	TSharedPtr<FJsonObject> HandleConnect(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDisconnect(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleConnectToOutput(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDisconnectOutput(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListConnections(const TSharedPtr<FJsonObject>& Params);

	// Expression property actions
	TSharedPtr<FJsonObject> HandleGetProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListProperties(const TSharedPtr<FJsonObject>& Params);

	// Parameter actions
	TSharedPtr<FJsonObject> HandlePromoteToParameter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateParameter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetParameterMetadata(const TSharedPtr<FJsonObject>& Params);

	// Material output actions
	TSharedPtr<FJsonObject> HandleGetOutputProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetOutputConnections(const TSharedPtr<FJsonObject>& Params);

	// Help action
	TSharedPtr<FJsonObject> HandleHelp(const TSharedPtr<FJsonObject>& Params);

	// Helper to load material from params
	class UMaterial* LoadMaterialFromParams(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutError);

	// Utility
	TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Code, const FString& Message);
	TSharedPtr<FJsonObject> CreateSuccessResponse();
	TSharedPtr<FJsonObject> ExpressionInfoToJson(const struct FMaterialExpressionInfo& Info);
	TSharedPtr<FJsonObject> TypeInfoToJson(const struct FMaterialExpressionTypeInfo& Info);
	TSharedPtr<FJsonObject> PinInfoToJson(const struct FMaterialPinInfo& Info);
	TSharedPtr<FJsonObject> ConnectionInfoToJson(const struct FMaterialConnectionInfo& Info);
};
