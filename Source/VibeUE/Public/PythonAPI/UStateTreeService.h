// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
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

	/** When the transition fires: "OnStateCompleted", "OnStateSucceeded", "OnStateFailed", "OnTick", "OnEvent", "OnDelegate" */
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

	/** Event payload struct type name (e.g. "FStartChasingPayload"), empty if none */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString EventPayloadStruct;

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

	/** Utility AI considerations for this state (StructType gives the consideration struct name, e.g. "FStateTreeConstantConsideration") */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	TArray<FStateTreeNodeInfo> Considerations;
};



/**
 * Info about a single editable task property discovered on either the task node struct or its instance data
 */
USTRUCT(BlueprintType)
struct FStateTreePropertyInfo
{
	GENERATED_BODY()

	/** Property name as it appears in C++ (case-sensitive; pass it to
	 * SetTransitionConditionPropertyValue, or to StateTreeToolset.set_node_instance_data) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Name;

	/** C++ type name, e.g. "FText", "float", "FLinearColor", "FVector", "bool", "FString" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Type;

	/** Current value exported to string (UE text format, e.g. "(R=1.0,G=0.0,B=0.0,A=1.0)" for FLinearColor) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString CurrentValue;

	/** Whether this exact property path is the target of a property binding (Python: is_bound) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bIsBound = false;

	/**
	 * Binding source when bound, formatted "<SourceKind>:<SourceStatePath/><SourceNodeName>.<SourcePropertyPath>",
	 * e.g. "RootParameter:Parameters.IdlingTime". Empty when unbound. A trailing " (output)" marks an output binding:
	 * the data flows the other way, this (Output) property WRITES the named source.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString BoundTo;
};

/**
 * One property binding in a StateTree. Internal to the binding reader that fills FStateTreePropertyInfo::bIsBound
 * and BoundTo; it is no longer returned by a tool. UE 5.9 has its own FStateTreeBindingInfo (StateTreeEditorTypes.h)
 * reachable as unreal.StateTreeEditorData.get_bindings(state), so the VibeUE name is prefixed to avoid the collision.
 * Kinds are strings. Target kinds: Task, Evaluator, GlobalTask, Condition, Consideration, Transition,
 * Parameter, PropertyFunction, Unknown. Source kinds: RootParameter, ContextData, Evaluator, GlobalTask,
 * Task, State (state parameters), Condition, Consideration, TransitionEvent, StateEvent, PropertyFunction,
 * Transition, Unknown.
 */
USTRUCT(BlueprintType)
struct FVibeUEStateTreeBindingInfo
{
	GENERATED_BODY()

	/** What owns the bound (target) property, e.g. "Task", "Condition", "Evaluator" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString TargetKind;

	/** State path of the target node (e.g. "Root/Chase"); empty for global nodes (evaluators, global tasks) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString TargetStatePath;

	/** Display name of the target node (e.g. "Delay", or the Blueprint task's display name) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString TargetNodeName;

	/**
	 * Struct name of the target node (e.g. "StateTreeDelayTask"). For Blueprint nodes (which live in a
	 * StateTreeBlueprint*Wrapper struct) this is the Blueprint class name instead (e.g. "STT_Idle_C"), the name
	 * the task functions accept. Empty for parameters, transitions and property functions.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString TargetNodeStruct;

	/**
	 * Array index of the target node in its owning list (state Tasks, EnterConditions, Considerations,
	 * a transition's Conditions, Transitions, global Evaluators or GlobalTasks). A state's SingleTask reports 0,
	 * like Tasks[0]. INDEX_NONE (-1) for parameters and property functions (the function node lives inside the
	 * binding). This is the raw array index, not the per-struct TaskMatchIndex used by the task functions.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	int32 TargetNodeIndex = INDEX_NONE;

	/** Index of the transition that owns the target (transition conditions and transitions); -1 otherwise */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	int32 TargetTransitionIndex = INDEX_NONE;

	/** Bound property path on the target node, e.g. "Duration" or "Offset.Z" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString TargetPropertyPath;

	/** What provides the value, e.g. "RootParameter", "ContextData", "Evaluator" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString SourceKind;

	/** State path of the source node; empty for global sources (root parameters, context, evaluators, global tasks) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString SourceStatePath;

	/** Name of the source struct: node display name, context name (e.g. "Actor"), or "Parameters" */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString SourceNodeName;

	/**
	 * Struct name of the source node when it is a node (evaluator, task, ...), with the same Blueprint rule as
	 * TargetNodeStruct. Empty for parameters, context data, events, transitions and property functions.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString SourceNodeStruct;

	/** Source property path, e.g. "IdlingTime"; empty when the whole source struct/object is bound */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString SourcePropertyPath;

	/** True when the source struct exists in this tree and the source path resolves against it (Python: source_resolves) */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bSourceResolves = false;

	/**
	 * Debug form of the binding: "<SourceStructID>:<SourcePropertyPath> -> <TargetStructID>:<TargetPropertyPath>",
	 * with " (output)" appended for output bindings. Struct IDs are FGuid::ToString() (32 hex digits); the paths are
	 * the same raw property-name paths as SourcePropertyPath / TargetPropertyPath (never editor display names), e.g.
	 * "C728B2074F954E16CAA196BE8447969C:WaitTime -> 9380F5F945B3E954A779FDA938C61610:Duration".
	 */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	FString Raw;

	/**
	 * True for an output binding (Python: is_output_binding): the engine copies TARGET -> SOURCE, i.e. the target
	 * node's Output property writes the source (e.g. a task output writing a root parameter). The compiler sets
	 * this when the target's root property is an Output property. False for ordinary SOURCE -> TARGET bindings.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "StateTree")
	bool bIsOutputBinding = false;
};


/**
 * StateTree Service - the 45 tools the engine's StateTree surface cannot reach. Python:
 * unreal.StateTreeService. State paths use "/" from the subtree root, e.g. "Root/Walking/Idle".
 *
 * Cut to the delta on the 5-9 branch, 98 tools down to 45. Use the engine for everything else:
 *  - states, tasks, enter conditions, considerations, evaluators, global tasks, compiling and the
 *    UI refresh: Epic's `StateTreeToolset`, a Python toolset reached with call_tool under
 *    snake_case names (add_child_state / add_sub_tree, add_task / add_blueprint_task,
 *    add_enter_condition, add_consideration, add_evaluator, add_global_task, each one's move_* and
 *    remove_* partner, compile, request_ui_refresh).
 *  - node data: `StateTreeToolset.get/set_node_instance_data` -- the per-node details-panel values,
 *    and the returned keys ARE the property names. get/set_node_template_data and
 *    get/set_node_execution_runtime_data reach the template and execution structs instead.
 *  - reading the tree: `get_editor_data`, then get_root_states / get_all_states / get_children.
 *  - state properties (Name, Description, Tag, Type, SelectionBehavior, TasksCompletion,
 *    LinkedSubtree, LinkedAsset, bEnabled, Weight): `ObjectToolset.SetProperties` on the
 *    UStateTreeState -- they are plain EditDefaultsOnly UPROPERTYs. Two carried-over behaviours:
 *    Weight is no longer clamped for you (ClampMin is only metadata on the JSON import path), and
 *    setting Type deliberately cascades (it resets Tasks, forces SelectionBehavior, clears links).
 *  - transitions: `ObjectToolset.SetProperties` appending one element to, or dropping one from,
 *    the state's Transitions array, which the engine applies incrementally. Pass a new
 *    transition's target state in the SAME write or the engine overwrites its Trigger, and never
 *    rewrite the whole array -- that remints every ID and orphans the bindings keyed on them.
 *  - the context actor class: `ObjectToolset.GetProperties` on the editor data for Schema's
 *    refPath, then SetProperties on THAT schema object with ContextActorClass. A nested
 *    Schema.ContextActorClass key makes the editor data the notify object, so the schema's
 *    PostEditChangeChainProperty never fires and ContextDataDescs never rebuilds.
 *  - finding and saving assets: `AssetToolset.FindAssets` / `SaveAsset` (Epic's writes only dirty
 *    the package: compile first, then save). Schemas: `ObjectToolset.SearchSubclasses` on
 *    UStateTreeSchema.
 * Epic's tools open no transaction of their own, so wrap a batch in
 * unreal.ScopedEditorTransaction if it should undo as one step. The `state-trees` skill carries
 * the full per-tool migration table.
 *
 * What is left is what none of those can reach:
 *  - all 24 property Bind* / Unbind* tools. Their engine counterpart, UStateTreeEditorData::
 *    SetBinding / ClearBinding, is PythonCallable native API, not an AI toolset tool.
 *  - CreateStateTree. Epic deliberately omits asset creation: UStateTreeFactory::SetSchemaClass is
 *    not a UFUNCTION, and its toolset v1 wraps only existing K2 APIs.
 *  - node-TYPE discovery: GetAvailableTaskTypes / GetAvailableConditionTypes /
 *    GetAvailableConsiderationTypes / GetAvailableEvaluatorTypes. Epic's adders take a
 *    UScriptStruct and nothing in the engine surface enumerates those (SearchSubclasses is
 *    UClass-only), so this is how you learn what to pass them.
 *  - theme colours: GetThemeColors / SetStateThemeColor / RenameThemeColor.
 *    FStateTreeEditorColor::ExportTextItem refuses an ordinary property read, and a state's
 *    ColorRef is only a GUID that needs GetThemeColors to resolve.
 *  - root parameters: AddOrUpdateRootParameter / RemoveRootParameter / RenameRootParameter, plus
 *    SetComponentParameterOverride. FInstancedPropertyBag's schema is C++-mutable only, and the
 *    override needs property-bag descriptor GUIDs nothing else exposes.
 *  - the not-path-addressable set: SetTaskEnabled, SetTaskConsideredForCompletion,
 *    AddTransitionCondition, GetTransitionConditionPropertyNames and
 *    SetTransitionConditionPropertyValue all live inside an FInstancedStruct or behind an
 *    Instanced object reference, which ToolsetPropertyPath will not cross; SetStateExpanded
 *    (bExpanded has neither CPF_Edit nor CPF_BlueprintVisible, so the toolset refuses it both ways
 *    and ListProperties cannot even see it); SelectState (SelectAssetEditorObjects raises for a
 *    non-Blueprint editor); and MoveTransition (no toolset emits ArrayMove, so a same-size rewrite
 *    would never notify the transition ViewModel).
 *  - GetTransitionEventPayloadPropertyNames, kept pending a live check of the engine's
 *    GetBindingOptions. Unverified either way; see the note on the function.
 */
UCLASS(BlueprintType)
class VIBEUE_API UStateTreeService : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	// ---- Discovery ----



	/**
	 * Get all registered task type names.
	 * Returns both struct-backed task types (e.g. "FStateTreeDelayTask") and blueprint task classes (e.g. "STT_Rotate_C").
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static TArray<FString> GetAvailableTaskTypes();

	/** Get all registered evaluator struct names discoverable from class iteration. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static TArray<FString> GetAvailableEvaluatorTypes();

	// ---- Asset Creation ----


	/**
	 * Create a new StateTree asset at the given content path.
	 * The path must include the asset name, e.g. "/Game/AI/MyStateTree"
	 * @param AssetPath       Content path including asset name
	 * @param SchemaClassName Schema class name (default: "StateTreeComponentSchema").
	 *                        Accepts full names or shorthand like "Component", "AIComponent".
	 *                        If creation returns False, the schema likely doesn't exist in this
	 *                        project -- run ObjectToolset.SearchSubclasses on UStateTreeSchema for
	 *                        the valid names.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool CreateStateTree(const FString& AssetPath, const FString& SchemaClassName = TEXT("StateTreeComponentSchema"));

	// ---- State Management ----









	/**
	 * Set a state's theme color by display name. Creates or updates the named color entry if needed.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool SetStateThemeColor(const FString& AssetPath, const FString& StatePath,
	                              const FString& ColorName, const FLinearColor& Color);

	/**
	 * Rename an existing theme color entry in the global color table.
	 * Preserves the ColorRef UUID so all states already using this color remain correctly linked.
	 * @param AssetPath    Content path to the StateTree
	 * @param OldColorName Current display name of the color entry (e.g. "Default Color")
	 * @param NewColorName New display name to assign (e.g. "Idle")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool RenameThemeColor(const FString& AssetPath, const FString& OldColorName, const FString& NewColorName);

	/**
	 * Get all theme colors defined in the StateTree's global color table.
	 * Returns each color's display name, color value, and which states reference it.
	 * @param AssetPath Content path to the StateTree
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static TArray<FStateTreeThemeColorInfo> GetThemeColors(const FString& AssetPath);

	/**
	 * Set whether a state is expanded or collapsed in the editor tree view.
	 * @param AssetPath Content path to the StateTree
	 * @param StatePath Path of the state (e.g. "Root" or "Root/Walking")
	 * @param bExpanded true to expand, false to collapse
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool SetStateExpanded(const FString& AssetPath, const FString& StatePath, bool bExpanded);

	/**
	 * Select a state in the editor tree view (highlights the state in the StateTree editor panel).
	 * The StateTree asset must already be open in an editor tab.
	 * Open it first with EditorAppToolset.OpenEditorForAsset if needed, then set_state_expanded to
	 * expand the parents, then select_state.
	 *
	 * @param AssetPath Content path to the StateTree (e.g. "/Game/AI/ST_Cube")
	 * @param StatePath Path of the state to select (e.g. "Root/Idle")
	 * @return true if the selection was applied successfully
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool SelectState(const FString& AssetPath, const FString& StatePath);




	// ---- State Properties ----






	// ---- Utility AI Considerations ----

	/** Get all registered consideration struct names (Constant, FloatInput, EnumInput, plus any custom). */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static TArray<FString> GetAvailableConsiderationTypes();





	/**
	 * Bind a consideration property to context data (e.g. for FloatInput.Input or EnumInput.Input).
	 * Leave ContextPropertyPath empty to bind the whole context object.
	 * @param MatchIndex  Which matching consideration to target. -1 means the last matching one.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindConsiderationPropertyToContext(const FString& AssetPath, const FString& StatePath,
	                                               const FString& ConsiderationStructName, const FString& ConsiderationPropertyPath,
	                                               const FString& ContextName = TEXT("Actor"),
	                                               const FString& ContextPropertyPath = TEXT(""),
	                                               int32 MatchIndex = -1);

	/**
	 * Bind a consideration property to a root parameter.
	 * @param MatchIndex  Which matching consideration to target. -1 means the last matching one.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindConsiderationPropertyToRootParameter(const FString& AssetPath, const FString& StatePath,
	                                                     const FString& ConsiderationStructName, const FString& ConsiderationPropertyPath,
	                                                     const FString& ParameterPath,
	                                                     int32 MatchIndex = -1);

	/** Remove the property binding on a consideration property (unbind it). */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool UnbindConsiderationProperty(const FString& AssetPath, const FString& StatePath,
	                                        const FString& ConsiderationStructName, const FString& ConsiderationPropertyPath,
	                                        int32 MatchIndex = -1);

	/** Set whether a task's completion contributes to the owning state's completion. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool SetTaskConsideredForCompletion(const FString& AssetPath, const FString& StatePath,
	                                           const FString& TaskStructName, int32 TaskMatchIndex, bool bConsideredForCompletion);

	// ---- Parameters ----


	/**
	 * Add or update a root parameter of any type.
	 * @param Type  "Bool", "Int32", "Int64", "Float", "Double", "Name", "String", "Text"
	 * @param DefaultValue  Initial/default value as a string (e.g. "3.14", "true", "Hello")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool AddOrUpdateRootParameter(const FString& AssetPath, const FString& Name,
	                                     const FString& Type, const FString& DefaultValue = TEXT(""));

	/** Remove a root parameter by name. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool RemoveRootParameter(const FString& AssetPath, const FString& Name);

	/** Rename a root parameter without breaking existing bindings. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool RenameRootParameter(const FString& AssetPath, const FString& OldName, const FString& NewName);

	// ---- Level Actor Component Parameter Overrides ----



	/**
	 * Set a per-instance parameter override on a placed actor's StateTreeComponent.
	 * The parameter type is resolved from the linked StateTree asset — no type argument needed.
	 * Supports all primitive types: Bool, Int32, Int64, Float, Double, Name, String, Text.
	 * Value format matches add_or_update_root_parameter: "3.14", "true", "Hello", etc.
	 * Save the level after setting overrides to persist them (AssetToolset.SaveAsset, or
	 * unreal.EditorLoadingAndSavingUtils from Python).
	 * @param ActorNameOrLabel  Name or label of the actor in the current level
	 * @param ParameterName     Name of the root parameter to override (must exist in the linked StateTree)
	 * @param Value             New value as a string (e.g. "3.14", "true", "Hello")
	 * @return true if the override was applied successfully
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool SetComponentParameterOverride(const FString& ActorNameOrLabel,
	                                          const FString& ParameterName,
	                                          const FString& Value);

	// ---- Transition Editing ----


	/**
	 * Bind an OnDelegate transition to a task's FStateTreeDelegateDispatcher property.
	 *
	 * Prerequisites:
	 *   1. The task must have a variable of type FStateTreeDelegateDispatcher (engine
	 *      BlueprintTools.add_variable, type_name "FStateTreeDelegateDispatcher").
	 *   2. The transition's Trigger must already be "OnDelegate" -- set it with
	 *      ObjectToolset.SetProperties on that element of the state's Transitions array.
	 *
	 * After calling this, compile the StateTree — a successful compile confirms the binding is valid.
	 *
	 * @param StatePath              Path of the state containing the transition (e.g. "Root/Rotating")
	 * @param TransitionIndex        Zero-based index into the state's Transitions array
	 *                               (ObjectToolset.GetProperties on the state, key Transitions)
	 * @param TaskStructName         Task name: display name, Blueprint name, or struct type (same as other task APIs)
	 * @param DispatcherPropertyName Name of the FStateTreeDelegateDispatcher variable on the task
	 * @param TaskMatchIndex         Which matching task to target (-1 = last match)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindTransitionToDelegate(const FString& AssetPath, const FString& StatePath,
	                                     int32 TransitionIndex, const FString& TaskStructName,
	                                     const FString& DispatcherPropertyName, int32 TaskMatchIndex = -1);


	/** Reorder a transition within the state's Transitions array. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool MoveTransition(const FString& AssetPath, const FString& StatePath, int32 FromIndex, int32 ToIndex);

	// ---- Task Management ----






	/**
	 * Bind a task property to a root parameter path (e.g. parameter "idling_time" -> task property "Duration").
	 * @param TaskMatchIndex Which matching task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindTaskPropertyToRootParameter(const FString& AssetPath, const FString& StatePath,
	                                           const FString& TaskStructName, const FString& TaskPropertyPath,
	                                           const FString& ParameterPath, int32 TaskMatchIndex = -1);

	/**
	 * Bind a task property to context data (e.g. context "Actor" path "debug_text" -> task property "BindableText").
	 * Leave ContextPropertyPath empty to bind the whole context object.
	 * @param TaskMatchIndex Which matching task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindTaskPropertyToContext(const FString& AssetPath, const FString& StatePath,
	                                     const FString& TaskStructName, const FString& TaskPropertyPath,
	                                     const FString& ContextName = TEXT("Actor"),
	                                     const FString& ContextPropertyPath = TEXT(""),
	                                     int32 TaskMatchIndex = -1);

	/**
	 * Bind a task property to a property exposed by a global task node.
	 * Use this when a state task should read data produced by a global task in the same StateTree.
	 * @param TaskMatchIndex Which matching state task to target for the struct type. -1 means the last matching task.
	 * @param GlobalTaskMatchIndex Which matching global task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindTaskPropertyToGlobalTaskProperty(const FString& AssetPath, const FString& StatePath,
	                                                const FString& TaskStructName, const FString& TaskPropertyPath,
	                                                const FString& GlobalTaskStructName, const FString& GlobalTaskPropertyPath,
	                                                int32 TaskMatchIndex = -1, int32 GlobalTaskMatchIndex = -1);

	/**
	 * Bind a task property to a property exposed by an evaluator node.
	 * Use this when a state task should read data produced by an evaluator in the same StateTree.
	 * @param TaskMatchIndex Which matching state task to target for the struct type. -1 means the last matching task.
	 * @param EvaluatorMatchIndex Which matching evaluator to target for the struct type. -1 means the last matching evaluator.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindTaskPropertyToEvaluatorProperty(const FString& AssetPath, const FString& StatePath,
	                                               const FString& TaskStructName, const FString& TaskPropertyPath,
	                                               const FString& EvaluatorStructName, const FString& EvaluatorPropertyPath,
	                                               int32 TaskMatchIndex = -1, int32 EvaluatorMatchIndex = -1);

	/**
	 * Remove the property binding on a task property (unbind it).
	 * After unbinding, the property reverts to its default/unbound value.
	 * @param TaskMatchIndex Which matching task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool UnbindTaskProperty(const FString& AssetPath, const FString& StatePath,
	                               const FString& TaskStructName, const FString& TaskPropertyPath,
	                               int32 TaskMatchIndex = -1);



	/** Enable or disable a task without removing it. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool SetTaskEnabled(const FString& AssetPath, const FString& StatePath,
	                           const FString& TaskStructName, int32 TaskMatchIndex, bool bEnabled);

	// ---- Evaluator / Global Task Management ----



	/** Get all registered condition struct names discoverable from class iteration. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static TArray<FString> GetAvailableConditionTypes();






	/**
	 * Bind an enter condition property to context data (e.g. context "Actor" path "TargetPawn" -> condition property "Object").
	 * Leave ContextPropertyPath empty to bind the whole context object.
	 * @param ConditionMatchIndex Which matching condition to target. -1 means the last matching condition.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindEnterConditionPropertyToContext(const FString& AssetPath, const FString& StatePath,
	                                                const FString& ConditionStructName, const FString& ConditionPropertyPath,
	                                                const FString& ContextName = TEXT("Actor"),
	                                                const FString& ContextPropertyPath = TEXT(""),
	                                                int32 ConditionMatchIndex = -1);

	/**
	 * Bind an enter condition property to a property exposed by a global task node.
	 * Use this when an enter condition should read data produced by a global task in the same StateTree.
	 * @param ConditionMatchIndex Which matching condition to target. -1 means the last matching condition.
	 * @param GlobalTaskMatchIndex Which matching global task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindEnterConditionPropertyToGlobalTaskProperty(const FString& AssetPath, const FString& StatePath,
	                                                          const FString& ConditionStructName, const FString& ConditionPropertyPath,
	                                                          const FString& GlobalTaskStructName, const FString& GlobalTaskPropertyPath,
	                                                          int32 ConditionMatchIndex = -1, int32 GlobalTaskMatchIndex = -1);

	/**
	 * Bind an enter condition property to a property exposed by an evaluator node.
	 * Use this when an enter condition should read data produced by an evaluator in the same StateTree.
	 * @param ConditionMatchIndex Which matching condition to target. -1 means the last matching condition.
	 * @param EvaluatorMatchIndex Which matching evaluator to target for the struct type. -1 means the last matching evaluator.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindEnterConditionPropertyToEvaluatorProperty(const FString& AssetPath, const FString& StatePath,
	                                                         const FString& ConditionStructName, const FString& ConditionPropertyPath,
	                                                         const FString& EvaluatorStructName, const FString& EvaluatorPropertyPath,
	                                                         int32 ConditionMatchIndex = -1, int32 EvaluatorMatchIndex = -1);

	/**
	 * Bind an enter condition property to a root parameter (e.g. parameter "CanChase" -> condition property "bLeft").
	 * @param ConditionMatchIndex Which matching condition to target. -1 means the last matching condition.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindEnterConditionPropertyToRootParameter(const FString& AssetPath, const FString& StatePath,
	                                                      const FString& ConditionStructName, const FString& ConditionPropertyPath,
	                                                      const FString& ParameterPath,
	                                                      int32 ConditionMatchIndex = -1);

	/** Remove the property binding on an enter condition property (unbind it). */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool UnbindEnterConditionProperty(const FString& AssetPath, const FString& StatePath,
	                                        const FString& ConditionStructName, const FString& ConditionPropertyPath,
	                                        int32 ConditionMatchIndex = -1);

	/** Add a condition to an existing transition. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool AddTransitionCondition(const FString& AssetPath, const FString& StatePath,
	                                   int32 TransitionIndex, const FString& ConditionStructName);



	/** Get all editable properties on a transition condition node. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static TArray<FStateTreePropertyInfo> GetTransitionConditionPropertyNames(const FString& AssetPath, const FString& StatePath,
	                                                                           int32 TransitionIndex, const FString& ConditionStructName,
	                                                                           int32 ConditionMatchIndex = -1);

	/**
	 * Get the bindable properties exposed by a transition's required event payload struct.
	 * Kept pending a live check of whether the engine's GetBindingOptions covers this; unverified
	 * either way, so prefer it over assuming an engine equivalent exists.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static TArray<FStateTreePropertyInfo> GetTransitionEventPayloadPropertyNames(const FString& AssetPath, const FString& StatePath,
	                                                                            int32 TransitionIndex);

	/** Set a property on a transition condition node. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool SetTransitionConditionPropertyValue(const FString& AssetPath, const FString& StatePath,
	                                                int32 TransitionIndex, const FString& ConditionStructName,
	                                                const FString& PropertyPath, const FString& Value,
	                                                int32 ConditionMatchIndex = -1);

	/**
	 * Bind a transition condition property to context data (e.g. context "Actor" path "TargetPawn" -> condition property "Object").
	 * Leave ContextPropertyPath empty to bind the whole context object.
	 * @param ConditionMatchIndex Which matching condition to target. -1 means the last matching condition.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindTransitionConditionPropertyToContext(const FString& AssetPath, const FString& StatePath,
	                                                     int32 TransitionIndex, const FString& ConditionStructName,
	                                                     const FString& ConditionPropertyPath,
	                                                     const FString& ContextName = TEXT("Actor"),
	                                                     const FString& ContextPropertyPath = TEXT(""),
	                                                     int32 ConditionMatchIndex = -1);

	/**
	 * Bind a transition condition property to an event payload property.
	 * The transition must have a RequiredEvent with a PayloadStruct set.
	 * @param PayloadPropertyPath Property path within the event payload struct (e.g. "TargetPawn"). The service resolves this against the payload struct and binds it through the transition event's Payload field.
	 * @param ConditionMatchIndex Which matching condition to target. -1 means the last matching condition.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindTransitionConditionPropertyToEventPayload(const FString& AssetPath, const FString& StatePath,
	                                                          int32 TransitionIndex, const FString& ConditionStructName,
	                                                          const FString& ConditionPropertyPath,
	                                                          const FString& PayloadPropertyPath,
	                                                          int32 ConditionMatchIndex = -1);

	/**
	 * Bind a transition condition property to a property exposed by an evaluator node.
	 * Use this when a transition condition should read data produced by an evaluator in the same StateTree.
	 * @param ConditionMatchIndex Which matching condition to target. -1 means the last matching condition.
	 * @param EvaluatorMatchIndex Which matching evaluator to target for the struct type. -1 means the last matching evaluator.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindTransitionConditionPropertyToEvaluatorProperty(const FString& AssetPath, const FString& StatePath,
	                                                              int32 TransitionIndex, const FString& ConditionStructName,
	                                                              const FString& ConditionPropertyPath,
	                                                              const FString& EvaluatorStructName, const FString& EvaluatorPropertyPath,
	                                                              int32 ConditionMatchIndex = -1, int32 EvaluatorMatchIndex = -1);

	/** Remove the property binding on a transition condition property (unbind it). */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool UnbindTransitionConditionProperty(const FString& AssetPath, const FString& StatePath,
	                                             int32 TransitionIndex, const FString& ConditionStructName,
	                                             const FString& ConditionPropertyPath,
	                                             int32 ConditionMatchIndex = -1);




	/**
	 * Bind an evaluator property to a root parameter path (e.g. parameter "PatrolTag" -> evaluator property "PatrolTag").
	 * Evaluators are global — no StatePath is needed.
	 * @param MatchIndex Which matching evaluator to target for the struct type. -1 means the last matching evaluator.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindEvaluatorPropertyToRootParameter(const FString& AssetPath, const FString& EvaluatorStructName,
	                                                 const FString& EvaluatorPropertyPath, const FString& ParameterPath,
	                                                 int32 MatchIndex = -1);

	/**
	 * Bind an evaluator property to context data (e.g. context "Actor" path "TargetPawn" -> evaluator property "ActorRef").
	 * Leave ContextPropertyPath empty to bind the whole context object.
	 * Evaluators are global — no StatePath is needed.
	 * @param MatchIndex Which matching evaluator to target for the struct type. -1 means the last matching evaluator.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindEvaluatorPropertyToContext(const FString& AssetPath, const FString& EvaluatorStructName,
	                                           const FString& EvaluatorPropertyPath,
	                                           const FString& ContextName = TEXT("Actor"),
	                                           const FString& ContextPropertyPath = TEXT(""),
	                                           int32 MatchIndex = -1);

	/** Remove the property binding on an evaluator property (unbind it). */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool UnbindEvaluatorProperty(const FString& AssetPath, const FString& EvaluatorStructName,
	                                    const FString& EvaluatorPropertyPath, int32 MatchIndex = -1);

	/**
	 * Bind a global task property to a root parameter path (e.g. parameter "PatrolTag" -> task property "PatrolTag").
	 * Global tasks are global — no StatePath is needed.
	 * @param MatchIndex Which matching global task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindGlobalTaskPropertyToRootParameter(const FString& AssetPath, const FString& TaskStructName,
	                                                  const FString& TaskPropertyPath, const FString& ParameterPath,
	                                                  int32 MatchIndex = -1);

	/**
	 * Bind a global task property to context data (e.g. context "Actor" path "TargetPawn" -> task property "ActorRef").
	 * Leave ContextPropertyPath empty to bind the whole context object.
	 * Global tasks are global — no StatePath is needed.
	 * @param MatchIndex Which matching global task to target for the struct type. -1 means the last matching task.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool BindGlobalTaskPropertyToContext(const FString& AssetPath, const FString& TaskStructName,
	                                            const FString& TaskPropertyPath,
	                                            const FString& ContextName = TEXT("Actor"),
	                                            const FString& ContextPropertyPath = TEXT(""),
	                                            int32 MatchIndex = -1);

	/** Remove the property binding on a global task property (unbind it). */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|StateTree")
	static bool UnbindGlobalTaskProperty(const FString& AssetPath, const FString& TaskStructName,
	                                     const FString& TaskPropertyPath, int32 MatchIndex = -1);




	// ---- Transitions ----


	// ---- Compile / Save ----


};
