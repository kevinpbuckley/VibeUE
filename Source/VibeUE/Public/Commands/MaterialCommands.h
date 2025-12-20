// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMaterialService;

/**
 * Command handler for Material operations.
 * Routes commands from the MCP bridge to FMaterialService.
 * 
 * Actions:
 * - create: Create a new material asset
 * - get_info: Get comprehensive material information
 * - list_properties: List all editable properties via reflection
 * - get_property: Get a property value
 * - get_property_info: Get detailed property metadata
 * - set_property: Set a property value
 * - set_properties: Set multiple properties at once
 * - list_parameters: List all material parameters
 * - get_parameter: Get a specific parameter
 * - set_parameter_default: Set a parameter's default value
 * - save: Save material to disk
 * - compile: Recompile material shaders
 */
class VIBEUE_API FMaterialCommands
{
public:
	FMaterialCommands();
	~FMaterialCommands();

	/**
	 * Handle a material command.
	 * 
	 * Expected params:
	 * {
	 *   "action": "create|get_info|list_properties|get_property|set_property|...",
	 *   "material_path": "/Game/Materials/M_MyMaterial",
	 *   ... action-specific parameters ...
	 * }
	 */
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FMaterialService> Service;

	// Lifecycle actions
	TSharedPtr<FJsonObject> HandleCreate(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateInstance(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSave(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCompile(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRefreshEditor(const TSharedPtr<FJsonObject>& Params);

	// Information actions
	TSharedPtr<FJsonObject> HandleGetInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSummarize(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListProperties(const TSharedPtr<FJsonObject>& Params);

	// Property actions
	TSharedPtr<FJsonObject> HandleGetProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetPropertyInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetProperties(const TSharedPtr<FJsonObject>& Params);

	// Parameter actions
	TSharedPtr<FJsonObject> HandleListParameters(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetParameter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetParameterDefault(const TSharedPtr<FJsonObject>& Params);

	// Instance information actions
	TSharedPtr<FJsonObject> HandleGetInstanceInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListInstanceProperties(const TSharedPtr<FJsonObject>& Params);

	// Instance property actions
	TSharedPtr<FJsonObject> HandleGetInstanceProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetInstanceProperty(const TSharedPtr<FJsonObject>& Params);

	// Instance parameter actions
	TSharedPtr<FJsonObject> HandleListInstanceParameters(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetInstanceScalarParameter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetInstanceVectorParameter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetInstanceTextureParameter(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleClearInstanceParameterOverride(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSaveInstance(const TSharedPtr<FJsonObject>& Params);

	// Help action
	TSharedPtr<FJsonObject> HandleHelp(const TSharedPtr<FJsonObject>& Params);

	// Utility
	TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Code, const FString& Message);
	TSharedPtr<FJsonObject> CreateSuccessResponse();
};
