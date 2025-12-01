// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Forward declarations for Enhanced Input services
class FEnhancedInputReflectionService;
class FInputActionService;
class FInputMappingService;

/**
 * Command handler for Enhanced Input system operations.
 * Routes commands from the MCP bridge to appropriate Enhanced Input services.
 * 
 * Supported services:
 * - reflection: Type and metadata discovery
 * - action: Input action lifecycle management
 * - mapping: Input mapping context management (includes modifier/trigger management)
 * 
 * Note: Modifier and trigger management has been consolidated into the mapping service.
 * Use mapping_add_modifier, mapping_remove_modifier, mapping_get_modifiers for modifiers.
 * Use mapping_add_trigger, mapping_remove_trigger, mapping_get_triggers for triggers.
 */
class FEnhancedInputCommands
{
public:
	FEnhancedInputCommands();
	~FEnhancedInputCommands();

	/**
	 * Main command handler - routes "manage_enhanced_input" commands.
	 * 
	 * Expected JSON params:
	 * {
	 *   "action": "action_name",       // lowercase, underscore-separated
	 *   "service": "service_name",     // reflection, action, mapping
	 *   ... additional service-specific parameters ...
	 * }
	 */
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Service instances
	TSharedPtr<FEnhancedInputReflectionService> ReflectionService;
	TSharedPtr<FInputActionService> ActionService;
	TSharedPtr<FInputMappingService> MappingService;

	// Service routing
	TSharedPtr<FJsonObject> RouteByService(const FString& Service, const FString& Action, const TSharedPtr<FJsonObject>& Params);

	// Individual service handlers
	TSharedPtr<FJsonObject> HandleReflectionService(const FString& Action, const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleActionService(const FString& Action, const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMappingService(const FString& Action, const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleModifierService(const FString& Action, const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleTriggerService(const FString& Action, const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAIService(const FString& Action, const TSharedPtr<FJsonObject>& Params);

	// Utility methods
	TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorCode, const FString& ErrorMessage);
	TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr);
};
