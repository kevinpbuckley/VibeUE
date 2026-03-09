// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UStateTreeService.generated.h"

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

	/** Global evaluators */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FStateTreeNodeInfo> Evaluators;

	/** Global tasks */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FStateTreeNodeInfo> GlobalTasks;

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

	/** Get all registered task struct names discoverable from class iteration. */
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

	/** Set ContextActorClass on component-style schemas (e.g. StateTreeComponentSchema / StateTreeAIComponentSchema). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetContextActorClass(const FString& AssetPath, const FString& ActorClassPath);

	/** Add or update a root float parameter and set its default value. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool AddOrUpdateRootFloatParameter(const FString& AssetPath, const FString& ParameterName,
	                                         float DefaultValue = 0.0f);

	// ---- Task Management ----

	/**
	 * Add a task to a state by C++ struct type name.
	 * @param StatePath      Path of the target state (e.g. "Root/Walking")
	 * @param TaskStructName FStateTreeTaskBase-derived struct name (e.g. "FStateTreeDelayTask")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool AddTask(const FString& AssetPath, const FString& StatePath,
	                    const FString& TaskStructName);

	/**
	 * Set a task instance-data property value using a property path (e.g. "Duration", "Text", "Offset.Z").
	 * @param TaskMatchIndex Which matching task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|StateTree")
	static bool SetTaskPropertyValue(const FString& AssetPath, const FString& StatePath,
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
