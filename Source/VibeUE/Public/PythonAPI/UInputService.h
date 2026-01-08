// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UInputService.generated.h"

/**
 * Information about an Input Action
 */
USTRUCT(BlueprintType)
struct FInputActionDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ActionName;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ActionPath;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ValueType;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bConsumeInput = true;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bTriggerWhenPaused = false;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString Description;
};

/**
 * Information about a Mapping Context
 */
USTRUCT(BlueprintType)
struct FMappingContextDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ContextName;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ContextPath;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	TArray<FString> MappedActions;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	int32 Priority = 0;
};

/**
 * Information about a key mapping in a context
 */
USTRUCT(BlueprintType)
struct FKeyMappingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	int32 MappingIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ActionName;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ActionPath;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString KeyName;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	int32 ModifierCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	int32 TriggerCount = 0;
};

/**
 * Information about a modifier on a mapping
 */
USTRUCT(BlueprintType)
struct FInputModifierInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	int32 ModifierIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString TypeName;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString DisplayName;
};

/**
 * Information about a trigger on a mapping
 */
USTRUCT(BlueprintType)
struct FInputTriggerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	int32 TriggerIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString TypeName;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString DisplayName;
};

/**
 * Result of creating an input action or mapping context
 */
USTRUCT(BlueprintType)
struct FInputCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FString ErrorMessage;
};

/**
 * Information about discovered input types
 */
USTRUCT(BlueprintType)
struct FInputTypeDiscoveryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	TArray<FString> ActionValueTypes;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	TArray<FString> ModifierTypes;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	TArray<FString> TriggerTypes;
};

/**
 * Input service exposed directly to Python (Enhanced Input System).
 *
 * Provides 19 enhanced input management actions:
 *
 * Reflection:
 * - reflection_discover_types: Discover available input action, modifier, and trigger types
 *
 * Action Management:
 * - action_create: Create a new Input Action asset
 * - action_list: List all Input Action assets
 * - action_get_properties: Get properties of an Input Action
 * - action_configure: Configure Input Action properties
 *
 * Mapping Context Management:
 * - mapping_create_context: Create a new Input Mapping Context
 * - mapping_list_contexts: List all Mapping Contexts
 * - mapping_get_mappings: Get all key mappings in a context
 * - mapping_add_key_mapping: Add a key mapping to a context
 * - mapping_remove_mapping: Remove a key mapping from a context
 * - mapping_get_available_keys: Get list of available input keys
 *
 * Modifier Management:
 * - mapping_add_modifier: Add a modifier to a key mapping
 * - mapping_remove_modifier: Remove a modifier from a key mapping
 * - mapping_get_modifiers: Get modifiers on a key mapping
 * - mapping_get_available_modifier_types: Get available modifier types
 *
 * Trigger Management:
 * - mapping_add_trigger: Add a trigger to a key mapping
 * - mapping_remove_trigger: Remove a trigger from a key mapping
 * - mapping_get_triggers: Get triggers on a key mapping
 * - mapping_get_available_trigger_types: Get available trigger types
 *
 * Python Usage:
 *   import unreal
 *
 *   # Discover available types
 *   types = unreal.InputService.discover_types()
 *
 *   # Create an input action
 *   result = unreal.InputService.create_action("IA_Jump", "/Game/Input", "Axis1D")
 *
 *   # Create a mapping context
 *   result = unreal.InputService.create_mapping_context("IMC_Default", "/Game/Input")
 *
 *   # Add key mapping
 *   unreal.InputService.add_key_mapping("/Game/Input/IMC_Default", "/Game/Input/IA_Jump", "SpaceBar")
 *
 * @note This replaces the JSON-based manage_enhanced_input MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UInputService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Reflection (reflection_discover_types)
	// =================================================================

	/**
	 * Discover available Enhanced Input types.
	 * Maps to action="reflection_discover_types"
	 *
	 * @return Discovery result with action value types, modifiers, and triggers
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static FInputTypeDiscoveryResult DiscoverTypes();

	// =================================================================
	// Action Management (action_create, action_list, action_get_properties, action_configure)
	// =================================================================

	/**
	 * Create a new Input Action asset.
	 * Maps to action="action_create"
	 *
	 * @param ActionName - Name for the new action
	 * @param AssetPath - Path where to create the asset (e.g., "/Game/Input")
	 * @param ValueType - Value type: "Digital", "Axis1D", "Axis2D", "Axis3D"
	 * @return Create result with asset path
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static FInputCreateResult CreateAction(
		const FString& ActionName,
		const FString& AssetPath,
		const FString& ValueType = TEXT("Axis1D"));

	/**
	 * List all Input Action assets.
	 * Maps to action="action_list"
	 *
	 * @return Array of Input Action paths
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static TArray<FString> ListInputActions();

	/**
	 * Get Input Action properties.
	 * Maps to action="action_get_properties"
	 *
	 * @param ActionPath - Full path to the Input Action
	 * @param OutInfo - Structure containing action details
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static bool GetInputActionInfo(const FString& ActionPath, FInputActionDetailedInfo& OutInfo);

	/**
	 * Configure an Input Action's properties.
	 * Maps to action="action_configure"
	 *
	 * @param ActionPath - Full path to the Input Action
	 * @param bConsumeInput - Whether the action consumes input
	 * @param bTriggerWhenPaused - Whether to trigger when game is paused
	 * @param Description - Description text for the action
	 * @return True if configuration was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static bool ConfigureAction(
		const FString& ActionPath,
		bool bConsumeInput = true,
		bool bTriggerWhenPaused = false,
		const FString& Description = TEXT(""));

	// =================================================================
	// Mapping Context Management
	// =================================================================

	/**
	 * Create a new Input Mapping Context.
	 * Maps to action="mapping_create_context"
	 *
	 * @param ContextName - Name for the new context
	 * @param AssetPath - Path where to create the asset
	 * @param Priority - Context priority (higher = processed first)
	 * @return Create result with asset path
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static FInputCreateResult CreateMappingContext(
		const FString& ContextName,
		const FString& AssetPath,
		int32 Priority = 0);

	/**
	 * List all Input Mapping Context assets.
	 * Maps to action="mapping_list_contexts"
	 *
	 * @return Array of Mapping Context paths
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static TArray<FString> ListMappingContexts();

	/**
	 * Get Mapping Context information including all mapped actions.
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param OutInfo - Structure containing context and mappings
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static bool GetMappingContextInfo(const FString& ContextPath, FMappingContextDetailedInfo& OutInfo);

	/**
	 * Get all key mappings in a context.
	 * Maps to action="mapping_get_mappings"
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @return Array of key mapping information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static TArray<FKeyMappingInfo> GetMappings(const FString& ContextPath);

	/**
	 * Add a key mapping to a context.
	 * Maps to action="mapping_add_key_mapping"
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param ActionPath - Full path to the Input Action
	 * @param KeyName - Name of the key (e.g., "SpaceBar", "W", "Gamepad_LeftTrigger")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static bool AddKeyMapping(
		const FString& ContextPath,
		const FString& ActionPath,
		const FString& KeyName);

	/**
	 * Remove a key mapping from a context.
	 * Maps to action="mapping_remove_mapping"
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static bool RemoveMapping(
		const FString& ContextPath,
		int32 MappingIndex);

	/**
	 * Get list of available input keys.
	 * Maps to action="mapping_get_available_keys"
	 *
	 * @param Filter - Optional filter string to narrow results
	 * @return Array of available key names
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static TArray<FString> GetAvailableKeys(const FString& Filter = TEXT(""));

	// =================================================================
	// Modifier Management
	// =================================================================

	/**
	 * Add a modifier to a key mapping.
	 * Maps to action="mapping_add_modifier"
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping
	 * @param ModifierType - Type of modifier (e.g., "Negate", "Scalar", "DeadZone")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static bool AddModifier(
		const FString& ContextPath,
		int32 MappingIndex,
		const FString& ModifierType);

	/**
	 * Remove a modifier from a key mapping.
	 * Maps to action="mapping_remove_modifier"
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping
	 * @param ModifierIndex - Index of the modifier to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static bool RemoveModifier(
		const FString& ContextPath,
		int32 MappingIndex,
		int32 ModifierIndex);

	/**
	 * Get modifiers on a key mapping.
	 * Maps to action="mapping_get_modifiers"
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping
	 * @return Array of modifier information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static TArray<FInputModifierInfo> GetModifiers(
		const FString& ContextPath,
		int32 MappingIndex);

	/**
	 * Get available modifier types.
	 * Maps to action="mapping_get_available_modifier_types"
	 *
	 * @return Array of modifier type names
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static TArray<FString> GetAvailableModifierTypes();

	// =================================================================
	// Trigger Management
	// =================================================================

	/**
	 * Add a trigger to a key mapping.
	 * Maps to action="mapping_add_trigger"
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping
	 * @param TriggerType - Type of trigger (e.g., "Pressed", "Released", "Down", "Hold")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static bool AddTrigger(
		const FString& ContextPath,
		int32 MappingIndex,
		const FString& TriggerType);

	/**
	 * Remove a trigger from a key mapping.
	 * Maps to action="mapping_remove_trigger"
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping
	 * @param TriggerIndex - Index of the trigger to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static bool RemoveTrigger(
		const FString& ContextPath,
		int32 MappingIndex,
		int32 TriggerIndex);

	/**
	 * Get triggers on a key mapping.
	 * Maps to action="mapping_get_triggers"
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping
	 * @return Array of trigger information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static TArray<FInputTriggerInfo> GetTriggers(
		const FString& ContextPath,
		int32 MappingIndex);

	/**
	 * Get available trigger types.
	 * Maps to action="mapping_get_available_trigger_types"
	 *
	 * @return Array of trigger type names
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Input")
	static TArray<FString> GetAvailableTriggerTypes();

private:
	/** Helper to load an Input Action */
	static class UInputAction* LoadInputAction(const FString& ActionPath);
	
	/** Helper to load a Mapping Context */
	static class UInputMappingContext* LoadMappingContext(const FString& ContextPath);
	
	/** Helper to find a key by name */
	static FKey FindKeyByName(const FString& KeyName);
};
