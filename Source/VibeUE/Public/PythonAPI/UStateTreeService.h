// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UStateTreeService.generated.h"

/**
 * Info about a root parameter in the StateTree property bag
 */
USTRUCT(BlueprintType)
struct FStateTreeParameterInfo
{
	GENERATED_BODY()

	/** Parameter name */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Name;

	/** Type string: "Bool", "Int32", "Int64", "Float", "Double", "Name", "String", "Text", "Enum", "Struct", "Object", "SoftObject", "Class", "SoftClass" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Type;

	/** Current default value exported as string */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString DefaultValue;
};

/**
 * Info about a single task, evaluator, or condition node inside a StateTree
 */
USTRUCT(BlueprintType)
struct FStateTreeNodeInfo
{
	GENERATED_BODY()

	/** Display name of the node */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Name;

	/** C++ struct type name (e.g. "FStateTreeDelayTask") */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString StructType;

	/** Whether this node is enabled */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bEnabled = true;

	/** Whether this task's completion contributes to the owning state's completion (tasks only) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bConsideredForCompletion = true;

	/** Condition operand: "Copy" (first), "And", "Or" (conditions only) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Operand;
};

/**
 * Info about a transition in a state
 */
USTRUCT(BlueprintType)
struct FStateTreeTransitionInfo
{
	GENERATED_BODY()

	/** When the transition fires: "OnStateCompleted", "OnStateSucceeded", "OnStateFailed", "OnTick", "OnEvent" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Trigger;

	/** What the transition does: "GotoState", "Succeeded", "Failed", "NextState", "NextSelectableState" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString TransitionType;

	/** Target state path (only for GotoState), e.g. "Root/Idle" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString TargetStatePath;

	/** Target state name (display) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString TargetStateName;

	/** Priority: "Low", "Normal", "Medium", "High", "Critical" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Priority;

	/** Whether this transition is enabled */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bEnabled = true;

	/** Zero-based index of this transition in the state's Transitions array */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	int32 Index = 0;

	/** Whether transition has a delay */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bDelayTransition = false;

	/** Delay duration in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	float DelayDuration = 0.0f;

	/** Random variance added to the delay duration */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	float DelayRandomVariance = 0.0f;

	/** Required gameplay event tag to trigger this transition (empty if none) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString RequiredEventTag;

	/** Conditions that must be true for this transition to fire */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FStateTreeNodeInfo> Conditions;

	/** Operand per condition: "Copy" (first), "And", "Or" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FString> ConditionOperands;
};

/**
 * Info about a theme color entry in a StateTree's global color table
 */
USTRUCT(BlueprintType)
struct FStateTreeThemeColorInfo
{
	GENERATED_BODY()

	/** Display name of the color entry (e.g. "Idle", "Rotation") */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString DisplayName;

	/** The color value */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FLinearColor Color = FLinearColor(0.4f, 0.4f, 0.4f);

	/** State paths that reference this color */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FString> UsedByStates;
};

/**
 * Info about a state in the StateTree
 */
USTRUCT(BlueprintType)
struct FStateTreeStateInfo
{
	GENERATED_BODY()

	/** Display name of the state */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Name;

	/** Full path from root, e.g. "Root/Walking/Idle" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Path;

	/** State type: "State", "Group", "Linked", "LinkedAsset", "Subtree" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString StateType;

	/** How children are selected: "TrySelectChildrenInOrder", "TrySelectChildrenAtRandom", etc. */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString SelectionBehavior;

	/** Whether this state is enabled */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bEnabled = true;

	/** Theme color display name assigned to this state (empty if none) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString ThemeColor;

	/** Whether this state is expanded in the editor tree view */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bExpanded = true;

	/** Tasks assigned to this state */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FStateTreeNodeInfo> Tasks;

	/** Enter conditions for this state */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FStateTreeNodeInfo> EnterConditions;

	/** Transitions from this state */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FStateTreeTransitionInfo> Transitions;

	/** Paths of direct child states */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FString> ChildPaths;

	/** Gameplay tag for this state (empty if none) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Tag;

	/** Editor description */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Description;

	/** Utility AI weight (used when parent SelectionBehavior is Utility-based) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	float Weight = 1.0f;

	/** Task completion mode: "Any" or "All" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString TasksCompletion;

	/** Whether this state uses a custom tick rate */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bHasCustomTickRate = false;

	/** Custom tick rate (ticks per second) if bHasCustomTickRate is true */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	float CustomTickRate = 0.0f;

	/** Required gameplay event tag to enter this state (empty if none) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString RequiredEventTag;

	/** Operand per enter condition: "Copy" (first), "And", "Or" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FString> EnterConditionOperands;
};

/**
 * Detailed info about a StateTree asset
 */
USTRUCT(BlueprintType)
struct FStateTreeInfo
{
	GENERATED_BODY()

	/** Content path to the asset */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString AssetPath;

	/** Asset name */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString AssetName;

	/** Schema class name */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString SchemaClass;

	/** Context actor class name (empty if not set — call set_context_actor_class to assign one) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString ContextActorClass;

	/** Global evaluators */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FStateTreeNodeInfo> Evaluators;

	/** Global tasks */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FStateTreeNodeInfo> GlobalTasks;

	/** Root parameters (property bag) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FStateTreeParameterInfo> RootParameters;

	/** All states (flattened hierarchy) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FStateTreeStateInfo> AllStates;

	/** Whether the asset has been successfully compiled */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bIsCompiled = false;

	/** Human-readable compile status */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString LastCompileStatus;
};

/**
 * Result of a StateTree compilation
 */
USTRUCT(BlueprintType)
struct FStateTreeCompileResult
{
	GENERATED_BODY()

	/** Whether compilation succeeded */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bSuccess = false;

	/** Error messages */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FString> Errors;

	/** Warning messages */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FString> Warnings;
};

/**
 * Info about a single editable task property discovered on either the task node struct or its instance data
 */
USTRUCT(BlueprintType)
struct FStateTreePropertyInfo
{
	GENERATED_BODY()

	/** Property name as it appears in C++ (case-sensitive, use this in set_task_property_value) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Name;

	/** C++ type name, e.g. "FText", "float", "FLinearColor", "FVector", "bool", "FString" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Type;

	/** Current value exported to string (UE text format, e.g. "(R=1.0,G=0.0,B=0.0,A=1.0)" for FLinearColor) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString CurrentValue;
};

/**
 * Detailed result of setting a task property value
 */
USTRUCT(BlueprintType)
struct FStateTreeTaskPropertySetResult
{
	GENERATED_BODY()

	/** Whether the property was set successfully */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bSuccess = false;

	/** Error message when the set fails */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString ErrorMessage;

	/** C++ type name of the resolved property */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString PropertyType;

	/** Value before the write, exported as a string */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString PreviousValue;

	/** Value after the write, exported as a string */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString NewValue;

	/** Which matching task instance was targeted for the struct type */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	int32 ResolvedTaskMatchIndex = INDEX_NONE;
};

/**
 * VibeUE service for creating, inspecting, and editing StateTree assets.
 * Exposed to Python as unreal.StateTreeService
 *
 * State paths use "/" separator starting from the subtree root name,
 * e.g. "Root", "Root/Walking", "Root/Walking/Idle"
 */
UCLASS(BlueprintType)
class VIBEUE_API UStateTreeService : public UObject
{
	GENERATED_BODY()

public:

	// ---- Discovery ----

	/** List all StateTree assets under a content directory. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FString> ListStateTrees(const FString& DirectoryPath = TEXT("/Game"));

	/** Get detailed structural info about a StateTree asset. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool GetStateTreeInfo(const FString& AssetPath, FStateTreeInfo& OutInfo);

	/**
	 * Get all registered task type names.
	 * Returns both struct-backed task types (e.g. "FStateTreeDelayTask") and blueprint task classes (e.g. "STT_Rotate_C").
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FString> GetAvailableTaskTypes();

	/** Get all registered evaluator struct names discoverable from class iteration. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FString> GetAvailableEvaluatorTypes();

	// ---- Asset Creation ----

	/**
	 * Create a new StateTree asset at the given content path.
	 * The path must include the asset name, e.g. "/Game/AI/MyStateTree"
	 * @param AssetPath       Content path including asset name
	 * @param SchemaClassName Schema class name (default: "StateTreeComponentSchema").
	 *                        Accepts full names or shorthand like "Component", "AIComponent".
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool CreateStateTree(const FString& AssetPath, const FString& SchemaClassName = TEXT("StateTreeComponentSchema"));

	// ---- State Management ----

	/**
	 * Add a state to the StateTree.
	 * @param AssetPath     Content path to the StateTree (e.g. "/Game/AI/MyStateTree")
	 * @param ParentPath    Path of parent state (e.g. "Root") or empty string to add a new top-level subtree
	 * @param StateName     Name for the new state
	 * @param StateType     "State" (default), "Group", "Subtree", "Linked", "LinkedAsset"
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool AddState(const FString& AssetPath, const FString& ParentPath,
	                     const FString& StateName, const FString& StateType = TEXT("State"));

	/** Remove a state and all its children by path. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool RemoveState(const FString& AssetPath, const FString& StatePath);

	/** Enable or disable a state. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetStateEnabled(const FString& AssetPath, const FString& StatePath, bool bEnabled);

	/** Set an editor description string for a state. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetStateDescription(const FString& AssetPath, const FString& StatePath, const FString& Description);

	/**
	 * Set a state's theme color by display name. Creates or updates the named color entry if needed.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetStateThemeColor(const FString& AssetPath, const FString& StatePath,
	                              const FString& ColorName, const FLinearColor& Color);

	/**
	 * Rename an existing theme color entry in the global color table.
	 * Preserves the ColorRef UUID so all states already using this color remain correctly linked.
	 * @param AssetPath    Content path to the StateTree
	 * @param OldColorName Current display name of the color entry (e.g. "Default Color")
	 * @param NewColorName New display name to assign (e.g. "Idle")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool RenameThemeColor(const FString& AssetPath, const FString& OldColorName, const FString& NewColorName);

	/**
	 * Get all theme colors defined in the StateTree's global color table.
	 * Returns each color's display name, color value, and which states reference it.
	 * @param AssetPath Content path to the StateTree
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FStateTreeThemeColorInfo> GetThemeColors(const FString& AssetPath);

	/**
	 * Set whether a state is expanded or collapsed in the editor tree view.
	 * @param AssetPath Content path to the StateTree
	 * @param StatePath Path of the state (e.g. "Root" or "Root/Walking")
	 * @param bExpanded true to expand, false to collapse
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetStateExpanded(const FString& AssetPath, const FString& StatePath, bool bExpanded);

	/** Set ContextActorClass on component-style schemas (e.g. StateTreeComponentSchema / StateTreeAIComponentSchema). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetContextActorClass(const FString& AssetPath, const FString& ActorClassPath);

	/** Add or update a root float parameter and set its default value. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool AddOrUpdateRootFloatParameter(const FString& AssetPath, const FString& ParameterName,
	                                         float DefaultValue = 0.0f);

	// ---- State Properties ----

	/** Set the selection behavior of a state (how children are selected). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetSelectionBehavior(const FString& AssetPath, const FString& StatePath, const FString& Behavior);

	/** Set task completion mode: "Any" (default) or "All". */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetTasksCompletion(const FString& AssetPath, const FString& StatePath, const FString& Completion);

	/** Rename a state. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool RenameState(const FString& AssetPath, const FString& StatePath, const FString& NewName);

	/** Set a gameplay tag on a state (pass empty string to clear). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetStateTag(const FString& AssetPath, const FString& StatePath, const FString& GameplayTag);

	/** Set the utility weight on a state (used when parent uses Utility-based selection). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetStateWeight(const FString& AssetPath, const FString& StatePath, float Weight);

	/** Set whether a task's completion contributes to the owning state's completion. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetTaskConsideredForCompletion(const FString& AssetPath, const FString& StatePath,
	                                           const FString& TaskStructName, int32 TaskMatchIndex, bool bConsideredForCompletion);

	// ---- Parameters ----

	/** Get all root parameters with their name, type, and current default value. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FStateTreeParameterInfo> GetRootParameters(const FString& AssetPath);

	/**
	 * Add or update a root parameter of any type.
	 * @param Type  "Bool", "Int32", "Int64", "Float", "Double", "Name", "String", "Text"
	 * @param DefaultValue  Initial/default value as a string (e.g. "3.14", "true", "Hello")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool AddOrUpdateRootParameter(const FString& AssetPath, const FString& Name,
	                                     const FString& Type, const FString& DefaultValue = TEXT(""));

	/** Remove a root parameter by name. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool RemoveRootParameter(const FString& AssetPath, const FString& Name);

	/** Rename a root parameter without breaking existing bindings. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool RenameRootParameter(const FString& AssetPath, const FString& OldName, const FString& NewName);

	// ---- Level Actor Component Parameter Overrides ----

	/**
	 * Get the game-content path of the StateTree asset linked to an actor's StateTreeComponent.
	 * Use this to discover parameters available to override before calling set_component_parameter_override.
	 * Then pass the returned path to get_root_parameters() to list all available parameter names and types.
	 * @param ActorNameOrLabel  Name or label of the actor in the current level
	 * @return Game path like "/Game/StateTrees/ST_Cube", or empty string if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static FString GetComponentStateTreePath(const FString& ActorNameOrLabel);

	/**
	 * Get the current parameter override values on a placed actor's StateTreeComponent.
	 * Returns the same structure as get_root_parameters but reflects instance-level values.
	 * Use this to inspect what values are set per-instance before or after calling set_component_parameter_override.
	 * @param ActorNameOrLabel  Name or label of the actor in the current level
	 * @return List of parameters with name, type, and current override value. Empty if actor or component not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FStateTreeParameterInfo> GetComponentParameterOverrides(const FString& ActorNameOrLabel);

	/**
	 * Set a per-instance parameter override on a placed actor's StateTreeComponent.
	 * The parameter type is resolved from the linked StateTree asset — no type argument needed.
	 * Supports all primitive types: Bool, Int32, Int64, Float, Double, Name, String, Text.
	 * Value format matches add_or_update_root_parameter: "3.14", "true", "Hello", etc.
	 * Call save_level (or use unreal.EditorLoadingAndSavingUtils) after setting overrides to persist.
	 * @param ActorNameOrLabel  Name or label of the actor in the current level
	 * @param ParameterName     Name of the root parameter to override (must exist in the linked StateTree)
	 * @param Value             New value as a string (e.g. "3.14", "true", "Hello")
	 * @return true if the override was applied successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetComponentParameterOverride(const FString& ActorNameOrLabel,
	                                          const FString& ParameterName,
	                                          const FString& Value);

	// ---- Transition Editing ----

	/**
	 * Update an existing transition. Empty string for Trigger/TransitionType/Priority means "don't change".
	 * @param TransitionIndex  Zero-based index in the state's Transitions array (from GetStateTreeInfo)
	 * @param Trigger          "OnStateCompleted", "OnStateSucceeded", "OnStateFailed", "OnTick", "OnEvent" — empty = no change
	 * @param TransitionType   "GotoState", "Succeeded", "Failed", "NextState", "NextSelectableState" — empty = no change
	 * @param TargetPath       Target state path, only used when TransitionType is "GotoState" — empty = no change
	 * @param Priority         "Low", "Normal", "Medium", "High", "Critical" — empty = no change
	 * @param bSetEnabled      Whether to update the enabled state
	 * @param bEnabled         New enabled value (only applied when bSetEnabled is true)
	 * @param bSetDelay        Whether to update delay settings
	 * @param bDelayTransition Whether the transition has a delay (only applied when bSetDelay is true)
	 * @param DelayDuration    Delay duration in seconds
	 * @param DelayRandomVariance Random variance added to delay duration
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool UpdateTransition(const FString& AssetPath, const FString& StatePath, int32 TransitionIndex,
	                             const FString& Trigger = TEXT(""), const FString& TransitionType = TEXT(""),
	                             const FString& TargetPath = TEXT(""), const FString& Priority = TEXT(""),
	                             bool bSetEnabled = false, bool bEnabled = true,
	                             bool bSetDelay = false, bool bDelayTransition = false,
	                             float DelayDuration = 0.0f, float DelayRandomVariance = 0.0f);

	/** Remove a transition by index. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool RemoveTransition(const FString& AssetPath, const FString& StatePath, int32 TransitionIndex);

	/** Reorder a transition within the state's Transitions array. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool MoveTransition(const FString& AssetPath, const FString& StatePath, int32 FromIndex, int32 ToIndex);

	// ---- Task Management ----

	/**
	 * Add a task to a state.
	 * @param StatePath      Path of the target state (e.g. "Root/Walking")
	 * @param TaskStructName Task type identifier. Supports:
	 *                       - Struct-backed task types (e.g. "FStateTreeDelayTask")
	 *                       - Blueprint task class names (e.g. "STT_Rotate_C")
	 *                       - Blueprint class object paths (e.g. "/Game/StateTree/STT_Rotate.STT_Rotate_C")
	 *                       - Blueprint asset paths (e.g. "/Game/StateTree/STT_Rotate")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool AddTask(const FString& AssetPath, const FString& StatePath,
	                    const FString& TaskStructName);

	/**
	 * Get all editable properties on a task, including properties declared directly on the task node struct
	 * and properties declared on its instance data, with name, type, and current value.
	 * Nested struct leaf properties are returned using dotted paths (e.g. "Offset.Z").
	 * Use this to discover property names before calling set_task_property_value — never guess.
	 * @param TaskMatchIndex Which matching task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FStateTreePropertyInfo> GetTaskPropertyNames(const FString& AssetPath, const FString& StatePath,
	                                                           const FString& TaskStructName, int32 TaskMatchIndex = -1);

	/**
	 * Read the current value of a single task property as a string.
	 * Returns empty string if the property path is invalid.
	 * @param TaskMatchIndex Which matching task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static FString GetTaskPropertyValue(const FString& AssetPath, const FString& StatePath,
	                                    const FString& TaskStructName, const FString& PropertyPath,
	                                    int32 TaskMatchIndex = -1);

	/**
	 * Set a task property value using a property path (e.g. "Duration", "Text", "Offset.Z").
	 * Call get_task_property_names first to discover valid property names — never guess.
	 * Use set_task_property_value_detailed if you need the failure reason and readback value in Python.
	 * @param TaskMatchIndex Which matching task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetTaskPropertyValue(const FString& AssetPath, const FString& StatePath,
	                                 const FString& TaskStructName, const FString& PropertyPath,
	                                 const FString& Value, int32 TaskMatchIndex = -1);

	/**
	 * Set a task property value and return a structured result with failure reason and readback value.
	 * Prefer this over the bool-only setter for agent-facing workflows.
	 * @param TaskMatchIndex Which matching task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static FStateTreeTaskPropertySetResult SetTaskPropertyValueDetailed(const FString& AssetPath, const FString& StatePath,
	                                                                   const FString& TaskStructName, const FString& PropertyPath,
	                                                                   const FString& Value, int32 TaskMatchIndex = -1);

	/**
	 * Bind a task property to a root parameter path (e.g. parameter "idling_time" -> task property "Duration").
	 * @param TaskMatchIndex Which matching task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool BindTaskPropertyToRootParameter(const FString& AssetPath, const FString& StatePath,
	                                           const FString& TaskStructName, const FString& TaskPropertyPath,
	                                           const FString& ParameterPath, int32 TaskMatchIndex = -1);

	/**
	 * Bind a task property to context data (e.g. context "Actor" path "debug_text" -> task property "BindableText").
	 * Leave ContextPropertyPath empty to bind the whole context object.
	 * @param TaskMatchIndex Which matching task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool BindTaskPropertyToContext(const FString& AssetPath, const FString& StatePath,
	                                     const FString& TaskStructName, const FString& TaskPropertyPath,
	                                     const FString& ContextName = TEXT("Actor"),
	                                     const FString& ContextPropertyPath = TEXT(""),
	                                     int32 TaskMatchIndex = -1);

	/**
	 * Remove the property binding on a task property (unbind it).
	 * After unbinding, the property reverts to its default/unbound value.
	 * @param TaskMatchIndex Which matching task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool UnbindTaskProperty(const FString& AssetPath, const FString& StatePath,
	                               const FString& TaskStructName, const FString& TaskPropertyPath,
	                               int32 TaskMatchIndex = -1);

	/** Remove a task from a state by struct type name. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool RemoveTask(const FString& AssetPath, const FString& StatePath,
	                       const FString& TaskStructName, int32 TaskMatchIndex = -1);

	/** Move a task to a different index within the state's Tasks array. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool MoveTask(const FString& AssetPath, const FString& StatePath,
	                     const FString& TaskStructName, int32 TaskMatchIndex, int32 NewIndex);

	/** Enable or disable a task without removing it. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetTaskEnabled(const FString& AssetPath, const FString& StatePath,
	                           const FString& TaskStructName, int32 TaskMatchIndex, bool bEnabled);

	// ---- Evaluator / Global Task Management ----

	/**
	 * Add a global evaluator to the StateTree by struct type name.
	 * @param EvaluatorStructName FStateTreeEvaluatorBase-derived struct name
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool AddEvaluator(const FString& AssetPath, const FString& EvaluatorStructName);

	/**
	 * Add a global task to the StateTree by struct type name.
	 * @param TaskStructName FStateTreeTaskBase-derived struct name
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool AddGlobalTask(const FString& AssetPath, const FString& TaskStructName);

	/** Get all registered condition struct names discoverable from class iteration. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FString> GetAvailableConditionTypes();

	/** Add an enter condition to a state. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool AddEnterCondition(const FString& AssetPath, const FString& StatePath, const FString& ConditionStructName);

	/** Remove an enter condition by index. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool RemoveEnterCondition(const FString& AssetPath, const FString& StatePath, int32 ConditionIndex);

	/** Set the And/Or operand on an enter condition. First condition should be "Copy", others "And" or "Or". */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetEnterConditionOperand(const FString& AssetPath, const FString& StatePath,
	                                     int32 ConditionIndex, const FString& Operand);

	/** Get all editable properties on an enter condition node. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FStateTreePropertyInfo> GetEnterConditionPropertyNames(const FString& AssetPath, const FString& StatePath,
	                                                                      const FString& ConditionStructName, int32 ConditionMatchIndex = -1);

	/** Set a property on an enter condition node. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetEnterConditionPropertyValue(const FString& AssetPath, const FString& StatePath,
	                                           const FString& ConditionStructName, const FString& PropertyPath,
	                                           const FString& Value, int32 ConditionMatchIndex = -1);

	/**
	 * Bind an enter condition property to context data (e.g. context "Actor" path "TargetPawn" -> condition property "Object").
	 * Leave ContextPropertyPath empty to bind the whole context object.
	 * @param ConditionMatchIndex Which matching condition to target. -1 means the last matching condition.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool BindEnterConditionPropertyToContext(const FString& AssetPath, const FString& StatePath,
	                                                const FString& ConditionStructName, const FString& ConditionPropertyPath,
	                                                const FString& ContextName = TEXT("Actor"),
	                                                const FString& ContextPropertyPath = TEXT(""),
	                                                int32 ConditionMatchIndex = -1);

	/**
	 * Bind an enter condition property to a root parameter (e.g. parameter "CanChase" -> condition property "bLeft").
	 * @param ConditionMatchIndex Which matching condition to target. -1 means the last matching condition.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool BindEnterConditionPropertyToRootParameter(const FString& AssetPath, const FString& StatePath,
	                                                      const FString& ConditionStructName, const FString& ConditionPropertyPath,
	                                                      const FString& ParameterPath,
	                                                      int32 ConditionMatchIndex = -1);

	/** Add a condition to an existing transition. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool AddTransitionCondition(const FString& AssetPath, const FString& StatePath,
	                                   int32 TransitionIndex, const FString& ConditionStructName);

	/** Remove a condition from a transition by condition index. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool RemoveTransitionCondition(const FString& AssetPath, const FString& StatePath,
	                                      int32 TransitionIndex, int32 ConditionIndex);

	/** Set the And/Or operand on a transition condition. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetTransitionConditionOperand(const FString& AssetPath, const FString& StatePath,
	                                          int32 TransitionIndex, int32 ConditionIndex, const FString& Operand);

	/** Get all editable properties on a transition condition node. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FStateTreePropertyInfo> GetTransitionConditionPropertyNames(const FString& AssetPath, const FString& StatePath,
	                                                                           int32 TransitionIndex, const FString& ConditionStructName,
	                                                                           int32 ConditionMatchIndex = -1);

	/** Set a property on a transition condition node. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetTransitionConditionPropertyValue(const FString& AssetPath, const FString& StatePath,
	                                                int32 TransitionIndex, const FString& ConditionStructName,
	                                                const FString& PropertyPath, const FString& Value,
	                                                int32 ConditionMatchIndex = -1);

	/**
	 * Bind a transition condition property to context data (e.g. context "Actor" path "TargetPawn" -> condition property "Object").
	 * Leave ContextPropertyPath empty to bind the whole context object.
	 * @param ConditionMatchIndex Which matching condition to target. -1 means the last matching condition.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool BindTransitionConditionPropertyToContext(const FString& AssetPath, const FString& StatePath,
	                                                     int32 TransitionIndex, const FString& ConditionStructName,
	                                                     const FString& ConditionPropertyPath,
	                                                     const FString& ContextName = TEXT("Actor"),
	                                                     const FString& ContextPropertyPath = TEXT(""),
	                                                     int32 ConditionMatchIndex = -1);

	/** Remove an evaluator by struct type name. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool RemoveEvaluator(const FString& AssetPath, const FString& EvaluatorStructName, int32 MatchIndex = -1);

	/** Get all editable properties on a global evaluator node. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FStateTreePropertyInfo> GetEvaluatorPropertyNames(const FString& AssetPath,
	                                                                  const FString& EvaluatorStructName, int32 MatchIndex = -1);

	/** Set a property on a global evaluator node. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetEvaluatorPropertyValue(const FString& AssetPath, const FString& EvaluatorStructName,
	                                      const FString& PropertyPath, const FString& Value, int32 MatchIndex = -1);

	/** Remove a global task by struct type name. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool RemoveGlobalTask(const FString& AssetPath, const FString& TaskStructName, int32 MatchIndex = -1);

	/** Get all editable properties on a global task node. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static TArray<FStateTreePropertyInfo> GetGlobalTaskPropertyNames(const FString& AssetPath,
	                                                                   const FString& TaskStructName, int32 MatchIndex = -1);

	/** Set a property on a global task node. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetGlobalTaskPropertyValue(const FString& AssetPath, const FString& TaskStructName,
	                                       const FString& PropertyPath, const FString& Value, int32 MatchIndex = -1);

	// ---- Transitions ----

	/**
	 * Add a transition to a state.
	 * @param StatePath      Path of the state to add the transition to (e.g. "Root/Walking")
	 * @param Trigger        "OnStateCompleted", "OnStateSucceeded", "OnStateFailed", "OnTick", "OnEvent"
	 * @param TransitionType "GotoState", "Succeeded", "Failed", "NextState", "NextSelectableState"
	 * @param TargetPath     Path of the target state (only used when TransitionType is "GotoState")
	 * @param Priority       "Low", "Normal", "Medium", "High", "Critical"
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool AddTransition(const FString& AssetPath, const FString& StatePath,
	                          const FString& Trigger, const FString& TransitionType,
	                          const FString& TargetPath = TEXT(""),
	                          const FString& Priority = TEXT("Normal"));

	// ---- Compile / Save ----

	/** Compile the StateTree asset. Must be called after structural changes. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static FStateTreeCompileResult CompileStateTree(const FString& AssetPath);

	/** Save the StateTree asset to disk. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SaveStateTree(const FString& AssetPath);
};
