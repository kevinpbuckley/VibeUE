// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FLevelActorService;

/**
 * Command handler for Level Actor operations.
 * Routes commands from the MCP bridge to LevelActorService.
 * 
 * Phase 1 Actions: add, remove, list, find, get_info
 * Phase 2 Actions: set_transform, get_transform, set_location, set_rotation, set_scale
 * Phase 3 Actions: get_property, set_property, get_all_properties
 * Phase 4 Actions: set_folder, attach, detach, select, rename
 */
class VIBEUE_API FLevelActorCommands
{
public:
	FLevelActorCommands();
	~FLevelActorCommands();

	/**
	 * Handle a level actor command.
	 * 
	 * Expected params:
	 * {
	 *   "action": "add|remove|list|find|get_info|set_transform|get_transform|set_location|set_rotation|set_scale",
	 *   ... action-specific parameters ...
	 * }
	 */
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FLevelActorService> Service;

	// Phase 1: Basic actor operations
	TSharedPtr<FJsonObject> HandleAdd(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemove(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleList(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFind(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetInfo(const TSharedPtr<FJsonObject>& Params);

	// Phase 2: Transform operations
	TSharedPtr<FJsonObject> HandleSetTransform(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetTransform(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLocation(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetRotation(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetScale(const TSharedPtr<FJsonObject>& Params);

	// Editor view operations
	TSharedPtr<FJsonObject> HandleFocus(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMoveToView(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRefreshViewport(const TSharedPtr<FJsonObject>& Params);

	// Phase 3: Property operations
	TSharedPtr<FJsonObject> HandleGetProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetAllProperties(const TSharedPtr<FJsonObject>& Params);

	// Phase 4: Hierarchy & Organization
	TSharedPtr<FJsonObject> HandleSetFolder(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAttach(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDetach(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSelect(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRename(const TSharedPtr<FJsonObject>& Params);

	// Utility
	TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Code, const FString& Message);
};
