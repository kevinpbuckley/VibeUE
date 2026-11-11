// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UInputService.generated.h"

/**
 * Information about an Input Action.
 * Python access: info = unreal.InputService.get_input_action_info(path)
 * 
 * Properties:
 * - action_name (str): Name of the action (e.g., "IA_Jump")
 * - action_path (str): Full asset path (e.g., "/Game/Input/IA_Jump")
 * - value_type (str): "Boolean", "Axis1D", "Axis2D", or "Axis3D"
 * - consume_input (bool): Whether the action consumes input
 * - trigger_when_paused (bool): Whether triggers when game is paused
 * - description (str): Description text for the action
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
 * Information about a Mapping Context.
 * Python access: info = unreal.InputService.get_mapping_context_info(path)
 * 
 * Properties:
 * - context_name (str): Name of the context (e.g., "IMC_Default")
 * - context_path (str): Full asset path (e.g., "/Game/Input/IMC_Default")
 * - mapped_actions (Array[str]): Paths of all mapped actions
 * - priority (int): Context priority (higher = processed first)
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
 * Information about a key mapping in a context.
 * Python access: mappings = unreal.InputService.get_mappings(context_path)
 * 
 * Properties:
 * - mapping_index (int): Index in the mapping context (0, 1, 2...)
 * - action_name (str): Name of the action (e.g., "IA_Jump")
 * - action_path (str): Full path to the action
 * - key_name (str): Key name (e.g., "SpaceBar", "Gamepad_RightTrigger")
 * - modifier_count (int): Number of modifiers on this mapping
 * - trigger_count (int): Number of triggers on this mapping
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
 * Information about a modifier on a mapping.
 * Python access: mods = unreal.InputService.get_modifiers(context_path, mapping_index)
 * 
 * Properties:
 * - modifier_index (int): Index in the modifier array (0, 1, 2...)
 * - type_name (str): Modifier class name (e.g., "InputModifierNegate", "InputModifierDeadZone")
 * - display_name (str): Human-readable display name
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
 * Information about a trigger on a mapping.
 * Python access: trigs = unreal.InputService.get_triggers(context_path, mapping_index)
 * 
 * Properties:
 * - trigger_index (int): Index in the trigger array (0, 1, 2...)
 * - type_name (str): Trigger class name (e.g., "InputTriggerPressed", "InputTriggerHold")
 * - display_name (str): Human-readable display name
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
 * Result of creating an input action or mapping context.
 * Python access: result = unreal.InputService.create_action(...)
 * 
 * Properties:
 * - success (bool): True if asset was created successfully
 * - asset_path (str): Full path to created asset (e.g., "/Game/Input/IA_Jump")
 * - error_message (str): Error message if failed (empty if success)
 * 
 * IMPORTANT: This is a struct, not a string. Use result.asset_path to get the path.
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
 * Information about discovered input types.
 * Python access: types = unreal.InputService.discover_types()
 * 
 * Properties:
 * - action_value_types (Array[str]): Available value types: "Boolean", "Axis1D", "Axis2D", "Axis3D"
 * - modifier_types (Array[str]): Available modifiers: "Negate", "DeadZone", "Scalar", etc.
 * - trigger_types (Array[str]): Available triggers: "Pressed", "Released", "Hold", "Tap", etc.
 * 
 * NOTE: Use action_value_types, NOT value_types!
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
 * Input Service - Enhanced Input ASSET authoring, plus action injection into PIE.
 *
 * Nothing in UE 5.9 authors Enhanced Input assets, so that whole surface stays here: Input Actions
 * and Mapping Contexts (create / list / info / configure / exists), key mappings, and the modifiers
 * and triggers on them -- addressed by INDEX on the mapping, so read with get_mappings /
 * get_modifiers / get_triggers before adding or removing. The reflection calls (discover_types,
 * get_available_keys, get_available_modifier_types, get_available_trigger_types) return the only
 * strings these calls accept; `search_symbols` / `get_symbol` give the live signatures.
 *
 * Use the engine for everything else:
 *  - pressing an action OR a raw key in a running session, for any duration: `InputToolset`
 *    (ApplyInput takes Entries -- each entry one Action or one Key, with its value -- and
 *    DurationSeconds; IsInputApplied reads back whether they are applied). This is what replaced
 *    VibeUE's held-action and raw-key injection.
 *  - any property on an input asset these calls do not reach: `EditorToolset.ObjectToolset`
 *    (GetProperties / SetProperties / ListProperties)
 *  - finding, saving and validating the assets: `EditorToolset.AssetToolset`
 *    (FindAssets / SaveAsset / ValidateAssets)
 *
 * On the PIE side what is left is inject_action: one Enhanced Input action value QUEUED onto the
 * next input tick of a PIE world's local player, with no OS focus and no remapping of game assets.
 * stop_injection is still a tool, but with the hold calls gone it has nothing of its own to release.
 *
 * Python Usage:
 *   import unreal
 *
 *   types  = unreal.InputService.discover_types()
 *   result = unreal.InputService.create_action("IA_Jump", "/Game/Input", "Axis1D")
 *   result = unreal.InputService.create_mapping_context("IMC_Default", "/Game/Input")
 *   unreal.InputService.add_key_mapping("/Game/Input/IMC_Default", "/Game/Input/IA_Jump", "SpaceBar")
 *   unreal.InputService.inject_action("/Game/Input/IA_Fire")
 */
UCLASS(BlueprintType)
class VIBEUE_API UInputService : public UToolsetDefinition
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
	static FInputTypeDiscoveryResult DiscoverTypes();

	// =================================================================
	// Action Management (action_create, action_list, action_get_properties, action_configure)
	// =================================================================

	/**
	 * Create a new Input Action asset.
	 * Maps to action="action_create"
	 *
	 * @param ActionName - Name for the new action. An invalid asset name (empty, or with a space, '.', '/' or another
	 *                     character asset names cannot hold) is refused with the reason
	 * @param AssetPath - Folder to create the asset in (e.g., "/Game/Input"). A path that starts with its mount point
	 *                    is kept; a bare folder ("Input") goes under /Game; a folder under no mounted content root
	 *                    (/Temp/X) is refused with the reason, and so are /Engine (the installed engine's content) and
	 *                    /Script. Plugin content roots are allowed
	 * @param ValueType - Value type: "Boolean" (alias "Digital"), "Axis1D", "Axis2D", "Axis3D" — same names discover_types returns
	 * @return Create result with asset path
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
	static TArray<FString> ListInputActions();

	/**
	 * Get Input Action properties.
	 * Maps to action="action_get_properties"
	 *
	 * @param ActionPath - Full path to the Input Action
	 * @param OutInfo - Structure containing action details
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	 * @param ContextName - Name for the new context, checked as create_action checks its name
	 * @param AssetPath - Folder to create the asset in, taken as create_action takes it
	 * @param Priority - Context priority (higher = processed first)
	 * @return Create result with asset path
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
	static TArray<FString> ListMappingContexts();

	/**
	 * Get Mapping Context information including all mapped actions.
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param OutInfo - Structure containing context and mappings
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
	static bool GetMappingContextInfo(const FString& ContextPath, FMappingContextDetailedInfo& OutInfo);

	/**
	 * Get all key mappings in a context.
	 * Maps to action="mapping_get_mappings"
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @return Array of key mapping information
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
	static TArray<FInputModifierInfo> GetModifiers(
		const FString& ContextPath,
		int32 MappingIndex);

	/**
	 * Get available modifier types.
	 * Maps to action="mapping_get_available_modifier_types"
	 *
	 * @return Array of modifier type names
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	 * @param PropertiesJson - Optional trigger settings as a JSON object, e.g.
	 *        {"HoldTimeThreshold": 0.5, "bIsOneShot": true}. Names match the C++ property or its
	 *        snake_case form (hold_time_threshold, is_one_shot). Only the settings the trigger's Details
	 *        panel shows can be set: runtime state (HeldDuration, LastValue, bShouldAlwaysTick, ...) is
	 *        refused, and a number outside the property's ClampMin/ClampMax (HoldTimeThreshold >= 0) is
	 *        refused, naming the range. An unknown name, a refused setting or a bad value fails the call
	 *        and adds nothing. A running PIE session keeps its copy of the triggers: restart PIE to see it.
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
	static bool AddTrigger(
		const FString& ContextPath,
		int32 MappingIndex,
		const FString& TriggerType,
		const FString& PropertiesJson = TEXT(""));

	/**
	 * Add a trigger to an Input Action's own trigger list (applies to every mapping of the action).
	 *
	 * @param ActionPath - Full path to the Input Action
	 * @param TriggerType - Type of trigger (e.g., "Pressed", "Hold", "Tap")
	 * @param PropertiesJson - Optional trigger settings as a JSON object, as for add_trigger
	 * @return JSON: {success, action, trigger, trigger_index, note} or {success:false, error_code, error_message}
	 *         error_code: PIE_ACTIVE (stop PIE first: assets cannot be loaded for editing during a play session),
	 *         ACTION_NOT_FOUND, TRIGGER_TYPE_NOT_FOUND, BAD_PROPERTIES
	 *
	 * Example:
	 *   unreal.InputService.add_action_trigger("/Game/Input/IA_Block", "Hold", '{"hold_time_threshold": 0.4}')
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
	static FString AddActionTrigger(
		const FString& ActionPath,
		const FString& TriggerType,
		const FString& PropertiesJson = TEXT(""));

	/**
	 * Remove a trigger from a key mapping.
	 * Maps to action="mapping_remove_trigger"
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping
	 * @param TriggerIndex - Index of the trigger to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
	static TArray<FInputTriggerInfo> GetTriggers(
		const FString& ContextPath,
		int32 MappingIndex);

	/**
	 * Get available trigger types.
	 * Maps to action="mapping_get_available_trigger_types"
	 *
	 * @return Array of trigger type names
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input")
	static TArray<FString> GetAvailableTriggerTypes();

	// =================================================================
	// Existence Checks
	// =================================================================

	/**
	 * Check if an Input Action exists at the given path.
	 *
	 * @param ActionPath - Full path to the Input Action
	 * @return True if Input Action exists
	 *
	 * Example:
	 *   if not unreal.InputService.input_action_exists("/Game/Input/IA_Jump"):
	 *       unreal.InputService.create_action("IA_Jump", "/Game/Input", "Axis1D")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input|Exists")
	static bool InputActionExists(const FString& ActionPath);

	/**
	 * Check if a Mapping Context exists at the given path.
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @return True if Mapping Context exists
	 *
	 * Example:
	 *   if not unreal.InputService.mapping_context_exists("/Game/Input/IMC_Default"):
	 *       unreal.InputService.create_mapping_context("IMC_Default", "/Game/Input")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input|Exists")
	static bool MappingContextExists(const FString& ContextPath);

	/**
	 * Check if a key mapping for a specific action exists in a context.
	 *
	 * @param ContextPath - Full path to the Mapping Context
	 * @param ActionPath - Full path to the Input Action
	 * @return True if a mapping for this action exists
	 *
	 * Example:
	 *   if not unreal.InputService.key_mapping_exists("/Game/Input/IMC_Default", "/Game/Input/IA_Jump"):
	 *       unreal.InputService.add_key_mapping("/Game/Input/IMC_Default", "/Game/Input/IA_Jump", "SpaceBar")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category ="VibeUE|Input|Exists")
	static bool KeyMappingExists(const FString& ContextPath, const FString& ActionPath);

	// =================================================================
	// PIE Input Injection (issue #550)
	// =================================================================

	/**
	 * Queue an Enhanced Input action value for the next input tick of a PIE world's local player.
	 * Removes the need to remap game input assets and send OS keystrokes just to test a mechanic.
	 *
	 * The value is QUEUED: it applies on the next input tick and is released on the tick after, so a
	 * press fires Started and Completed almost at once. The world does not tick while a Python call runs,
	 * so read the result in a LATER call, two or more frames on. To HOLD an action (a guard, a charge,
	 * a Hold trigger) use the engine's `InputToolset` ApplyInput with a DurationSeconds -- holding is no
	 * longer done here. X/Y/Z map onto the action's value type (Boolean uses X != 0).
	 *
	 * @param ActionPath - Input Action asset path (/Game/Input/IA_Fire or full object path)
	 * @param PieInstance - Which PIE world (its PIE instance number); -1 = the first PIE world with a local
	 *        player (lowest instance number: the listen server's window, or client 1 under a dedicated server)
	 * @return JSON: {success, queued, action, value_type, injected:[x,y,z], pie_instance (the instance used)} or
	 *         {success:false, error_code, error_message}
	 *         error_code: PIE_NOT_RUNNING, NO_PIE_INSTANCE, NO_LOCAL_PLAYER, NO_PLAYER_CONTROLLER (PIE start or a
	 *         client's join is still in progress: retry on a later tick), NO_ENHANCED_INPUT, ACTION_NOT_FOUND
	 *
	 * Example:
	 *   unreal.InputService.inject_action("/Game/Input/IA_Fire_Secondary")   # fire once
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Input|PIE")
	static FString InjectAction(const FString& ActionPath, float X = 1.0f, float Y = 0.0f, float Z = 0.0f, int32 PieInstance = -1);

	/**
	 * Start holding an action INDEFINITELY, until stop_injection releases it or PIE ends. No duration.
	 *
	 * This is the one input hold the engine cannot express. `InputToolset.ApplyInput` takes a
	 * DurationSeconds and returns an async result that completes only once the hold has ended, so the
	 * hold and the agent's turn are the same span: you cannot hold an input and then do anything else
	 * while it is held. Use ApplyInput for any hold of known length (it also presses several
	 * actions/keys together, which this does not) and use this when the release depends on something
	 * you have to observe first -- hold forward, read where the character got to, then release.
	 *
	 * Calling it twice for the same action and PIE world restarts the hold rather than stacking.
	 * The hold registers in this service's table, so shutdown_injection_hooks and the EndPIE hook
	 * both drop it; a hold can never outlive its PIE session.
	 *
	 * @param ActionPath - Input Action asset path (/Game/Input/IA_MoveForward or full object path)
	 * @param X,Y,Z - The held value; maps onto the action's value type (Boolean uses X != 0)
	 * @param PieInstance - Which PIE world, as for inject_action; -1 = the first PIE world with a local player
	 * @return JSON: {success, action, pie_instance (the instance used)} or
	 *         {success:false, error_code, error_message}; same error codes as inject_action.
	 *
	 * Example:
	 *   unreal.InputService.start_injection("/Game/Input/IA_MoveForward")   # walk
	 *   # ... other tool calls; read the pawn's location ...
	 *   unreal.InputService.stop_injection("/Game/Input/IA_MoveForward")    # stop walking
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Input|PIE")
	static FString StartInjection(const FString& ActionPath, float X = 1.0f, float Y = 0.0f, float Z = 0.0f, int32 PieInstance = -1);

	/**
	 * Release an action hold filed by start_injection. The hold is found by the action it names and the
	 * PIE world, so /Game/Input/IA_X and /Game/Input/IA_X.IA_X are the same hold.
	 *
	 * A hold started with the engine's `InputToolset` ApplyInput is Epic's own, is not visible here,
	 * and ends on its own DurationSeconds.
	 *
	 * @param ActionPath - Input Action asset path
	 * @param PieInstance - Which PIE world, as for inject_action; -1 = the first PIE world with a local player
	 * @return JSON: {success, action, was_active, pie_instance (the instance used)} or
	 *         {success:false, error_code, error_message}. was_active is false if nothing was held
	 *         under that action, and once PIE has ended.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Input|PIE")
	static FString StopInjection(const FString& ActionPath, int32 PieInstance = -1);


	/**
	 * End every action and key hold this service still has filed (injections stopped, pending releases and
	 * the release grace cancelled; no key-up is sent) and unhook the FEditorDelegates::EndPIE handler and the
	 * GEditor->ShouldDisableCPUThrottlingDelegates entry the holds registered. Called from the module's
	 * OnPreExit (while GEditor still exists) and ShutdownModule. Both callbacks, and the hold tickers, are
	 * code INTO THIS DLL, so they must not outlive an unload (plugin reload / Live Coding) — the same
	 * teardown as UPIEActorService::ShutdownEndPIEHook. Safe to call twice and with GEditor null (then the
	 * throttling handle is only reset: the array went with the editor engine). Not a UFUNCTION.
	 * GAME-THREAD ONLY.
	 */
	static void ShutdownInjectionHooks();

private:
	/** Helper to load an Input Action */
	static class UInputAction* LoadInputAction(const FString& ActionPath);
	
	/** Helper to load a Mapping Context */
	static class UInputMappingContext* LoadMappingContext(const FString& ContextPath);
	
	/** Helper to find a key by name */
	static FKey FindKeyByName(const FString& KeyName);
};
