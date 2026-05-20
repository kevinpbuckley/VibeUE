// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UAnimGraphService.generated.h"

/**
 * Information about a state machine within an Animation Blueprint
 */
USTRUCT(BlueprintType)
struct FAnimStateMachineInfo
{
	GENERATED_BODY()

	/** Name of the state machine */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString MachineName;

	/** Node GUID for identification */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString NodeId;

	/** Number of states in this machine */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 StateCount = 0;

	/** Name of the graph containing this state machine */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString ParentGraphName;
};

/**
 * Information about a state within a state machine
 */
USTRUCT(BlueprintType)
struct FAnimStateInfo
{
	GENERATED_BODY()

	/** Name of the state */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString StateName;

	/** Node GUID for identification */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString NodeId;

	/** Type of state: "State", "Conduit", "Entry", "AliasedState" */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString StateType;

	/** Whether this is an end/terminal state */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bIsEndState = false;

	/** X position in the graph */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float PosX = 0.0f;

	/** Y position in the graph */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float PosY = 0.0f;
};

/**
 * Information about a transition between states
 */
USTRUCT(BlueprintType)
struct FAnimTransitionInfo
{
	GENERATED_BODY()

	/** Name/title of the transition */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString TransitionName;

	/** Node GUID for identification */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString NodeId;

	/** Name of the source state */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString SourceState;

	/** Name of the destination state */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString DestState;

	/** Transition priority (lower = higher priority) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 Priority = 0;

	/** Blend duration in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float BlendDuration = 0.0f;

	/** Whether this is an automatic transition */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bIsAutomatic = false;

	/** Classification of the rule driving this transition: "None" (inert), "Bool", "Comparison", "Automatic", "Custom" */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString RuleType;

	/** Name of the bound variable driving the rule (if RuleType is "Bool" or "Comparison"), else empty */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString RuleVariable;

	/** Human-readable description of the rule (e.g. "bIsDead == true", "Speed > 150", "auto (time remaining)") */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString RuleSummary;

	/** True when the transition has functional rule logic and can actually fire. False = inert (never fires). */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bHasRule = false;
};

/**
 * Result of validating a state machine for common authoring mistakes.
 */
USTRUCT(BlueprintType)
struct FAnimStateMachineValidationResult
{
	GENERATED_BODY()

	/** True when there are no blocking errors (warnings may still be present) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bIsValid = false;

	/** Number of states found (excludes entry/conduit) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 StateCount = 0;

	/** Number of transitions found */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 TransitionCount = 0;

	/** Blocking problems that make the machine non-functional (e.g. inert transitions, no entry state) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	TArray<FString> Errors;

	/** Non-blocking issues worth attention (e.g. unreachable states, states with no animation) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	TArray<FString> Warnings;
};

/**
 * Information about an animation graph (AnimGraph, state graph, etc.)
 */
USTRUCT(BlueprintType)
struct FAnimGraphInfo
{
	GENERATED_BODY()

	/** Name of the graph */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString GraphName;

	/** Type of graph: "AnimGraph", "StateMachine", "State", "Transition", "EventGraph" */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString GraphType;

	/** Number of nodes in this graph */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 NodeCount = 0;

	/** Parent graph name (if this is a sub-graph) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString ParentGraphName;
};

/**
 * Information about an animation sequence reference in an AnimBP
 */
USTRUCT(BlueprintType)
struct FAnimSequenceUsageInfo
{
	GENERATED_BODY()

	/** Asset path of the animation sequence */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString SequencePath;

	/** Display name of the sequence */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString SequenceName;

	/** Graph where this sequence is used */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString UsedInGraph;

	/** Node that uses this sequence */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString UsedByNode;
};

/**
 * Animation Graph service exposed directly to Python.
 *
 * This service provides AnimBlueprint introspection and navigation functionality,
 * including the ability to open specific state machines and states in the editor.
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all state machines in an AnimBP
 *   machines = unreal.AnimGraphService.list_state_machines("/Game/ABP_Character")
 *   for m in machines:
 *       print(f"Machine: {m.machine_name}, States: {m.state_count}")
 *
 *   # List states in a state machine
 *   states = unreal.AnimGraphService.list_states_in_machine("/Game/ABP_Character", "Locomotion")
 *   for s in states:
 *       print(f"State: {s.state_name} ({s.state_type})")
 *
 *   # Open a specific state in the editor
 *   unreal.AnimGraphService.open_anim_state("/Game/ABP_Character", "Locomotion", "IdleLoop")
 *
 * @note All methods are static and thread-safe
 * @note C++ out parameters become Python return values
 *
 * **C++ Source:**
 *
 * - **Plugin**: VibeUE
 * - **Module**: VibeUE
 * - **File**: UAnimGraphService.h
 */
UCLASS(BlueprintType)
class VIBEUE_API UAnimGraphService : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================================
	// GRAPH NAVIGATION
	// ============================================================================

	/**
	 * Open an Animation Blueprint and focus on a specific graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph to open (default: "AnimGraph")
	 * @return True if the graph was opened successfully
	 *
	 * Example:
	 *   unreal.AnimGraphService.open_anim_graph("/Game/ABP_Character", "AnimGraph")
	 *   unreal.AnimGraphService.open_anim_graph("/Game/ABP_Character", "Locomotion")  # state machine
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Navigation")
	static bool OpenAnimGraph(
		const FString& AnimBlueprintPath,
		const FString& GraphName = TEXT("AnimGraph"));

	/**
	 * Open a specific state within a state machine in the editor.
	 * This navigates directly to the state's internal graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine containing the state
	 * @param StateName - Name of the state to open
	 * @return True if the state was opened successfully
	 *
	 * Example:
	 *   unreal.AnimGraphService.open_anim_state("/Game/ABP_Character", "Locomotion", "IdleLoop")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Navigation")
	static bool OpenAnimState(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& StateName);

	/**
	 * Open a transition rule between two states in the editor.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine containing the transition
	 * @param SourceStateName - Name of the source state
	 * @param DestStateName - Name of the destination state
	 * @return True if the transition was opened successfully
	 *
	 * Example:
	 *   unreal.AnimGraphService.open_transition("/Game/ABP_Character", "Locomotion", "Idle", "Walk")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Navigation")
	static bool OpenTransition(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& SourceStateName,
		const FString& DestStateName);

	/**
	 * Focus on a specific node by its GUID in any graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param NodeId - GUID of the node to focus on
	 * @return True if the node was found and focused
	 *
	 * Example:
	 *   unreal.AnimGraphService.focus_node("/Game/ABP_Character", "ABC123...")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Navigation")
	static bool FocusNode(
		const FString& AnimBlueprintPath,
		const FString& NodeId);

	// ============================================================================
	// GRAPH INTROSPECTION
	// ============================================================================

	/**
	 * List all graphs in an Animation Blueprint.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @return Array of graph information
	 *
	 * Example:
	 *   graphs = unreal.AnimGraphService.list_graphs("/Game/ABP_Character")
	 *   for g in graphs:
	 *       print(f"{g.graph_name} ({g.graph_type}): {g.node_count} nodes")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Introspection")
	static TArray<FAnimGraphInfo> ListGraphs(const FString& AnimBlueprintPath);

	/**
	 * List all state machines in an Animation Blueprint.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @return Array of state machine information
	 *
	 * Example:
	 *   machines = unreal.AnimGraphService.list_state_machines("/Game/ABP_Character")
	 *   for m in machines:
	 *       print(f"Machine: {m.machine_name}, States: {m.state_count}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Introspection")
	static TArray<FAnimStateMachineInfo> ListStateMachines(const FString& AnimBlueprintPath);

	/**
	 * List all states within a specific state machine.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @return Array of state information
	 *
	 * Example:
	 *   states = unreal.AnimGraphService.list_states_in_machine("/Game/ABP_Character", "Locomotion")
	 *   for s in states:
	 *       print(f"State: {s.state_name} ({s.state_type})")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Introspection")
	static TArray<FAnimStateInfo> ListStatesInMachine(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName);

	/**
	 * Get all transitions for a specific state.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param StateName - Name of the state (empty for all transitions in machine)
	 * @return Array of transition information
	 *
	 * Example:
	 *   transitions = unreal.AnimGraphService.get_state_transitions("/Game/ABP_Character", "Locomotion", "Idle")
	 *   for t in transitions:
	 *       print(f"{t.source_state} -> {t.dest_state}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Introspection")
	static TArray<FAnimTransitionInfo> GetStateTransitions(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& StateName = TEXT(""));

	/**
	 * Get detailed information about a specific state machine.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param OutInfo - Output state machine info
	 * @return True if the state machine was found
	 *
	 * Example:
	 *   info = unreal.AnimGraphService.get_state_machine_info("/Game/ABP_Character", "Locomotion")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Introspection")
	static bool GetStateMachineInfo(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		FAnimStateMachineInfo& OutInfo);

	/**
	 * Get detailed information about a specific state.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param StateName - Name of the state
	 * @param OutInfo - Output state info
	 * @return True if the state was found
	 *
	 * Example:
	 *   info = unreal.AnimGraphService.get_state_info("/Game/ABP_Character", "Locomotion", "Idle")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Introspection")
	static bool GetStateInfo(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& StateName,
		FAnimStateInfo& OutInfo);

	// ============================================================================
	// ANIMATION ASSET ANALYSIS
	// ============================================================================

	/**
	 * Get all animation sequences used in an Animation Blueprint.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @return Array of animation sequence usage information
	 *
	 * Example:
	 *   sequences = unreal.AnimGraphService.get_used_anim_sequences("/Game/ABP_Character")
	 *   for seq in sequences:
	 *       print(f"{seq.sequence_name} used in {seq.used_in_graph}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Assets")
	static TArray<FAnimSequenceUsageInfo> GetUsedAnimSequences(const FString& AnimBlueprintPath);

	/**
	 * Get the skeleton used by an Animation Blueprint.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @return Path to the skeleton asset, or empty string if not found
	 *
	 * Example:
	 *   skeleton_path = unreal.AnimGraphService.get_skeleton("/Game/ABP_Character")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Assets")
	static FString GetSkeleton(const FString& AnimBlueprintPath);

	/**
	 * Get the preview mesh used by an Animation Blueprint.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @return Path to the preview skeletal mesh, or empty string if not found
	 *
	 * Example:
	 *   mesh_path = unreal.AnimGraphService.get_preview_mesh("/Game/ABP_Character")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Assets")
	static FString GetPreviewMesh(const FString& AnimBlueprintPath);

	// ============================================================================
	// UTILITY
	// ============================================================================

	/**
	 * Check if an asset is an Animation Blueprint.
	 *
	 * @param AssetPath - Full path to the asset
	 * @return True if the asset is an Animation Blueprint
	 *
	 * Example:
	 *   if unreal.AnimGraphService.is_anim_blueprint("/Game/ABP_Character"):
	 *       print("It's an AnimBP!")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Utility")
	static bool IsAnimBlueprint(const FString& AssetPath);

	/**
	 * Get the parent class of an Animation Blueprint.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @return Name of the parent class (e.g., "AnimInstance")
	 *
	 * Example:
	 *   parent = unreal.AnimGraphService.get_parent_class("/Game/ABP_Character")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Utility")
	static FString GetParentClass(const FString& AnimBlueprintPath);

	// ============================================================================
	// ANIMGRAPH NODE CREATION
	// ============================================================================

	/**
	 * Add a state machine node to the AnimGraph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param MachineName - Name for the new state machine
	 * @param PosX - X position in AnimGraph (default 0)
	 * @param PosY - Y position in AnimGraph (default 0)
	 * @return Node GUID if successful, empty string otherwise
	 *
	 * Example:
	 *   node_id = unreal.AnimGraphService.add_state_machine("/Game/ABP_Character", "Locomotion")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString AddStateMachine(
		const FString& AnimBlueprintPath,
		const FString& MachineName,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a sequence player node to a graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph (AnimGraph, state name, etc.)
	 * @param AnimSequencePath - Optional path to animation sequence
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @return Node GUID if successful, empty string otherwise
	 *
	 * Example:
	 *   node_id = unreal.AnimGraphService.add_sequence_player(
	 *       "/Game/ABP_Character",
	 *       "IdleState",
	 *       "/Game/Animations/Idle_Loop"
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString AddSequencePlayer(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& AnimSequencePath = TEXT(""),
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a blend space player node to a graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param BlendSpacePath - Optional path to blend space asset
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @return Node GUID if successful
	 *
	 * Example:
	 *   node_id = unreal.AnimGraphService.add_blend_space_player(
	 *       "/Game/ABP_Character",
	 *       "MovingState",
	 *       "/Game/Animations/BS_Locomotion"
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString AddBlendSpacePlayer(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& BlendSpacePath = TEXT(""),
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Blend By Bool node to a graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @return Node GUID if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString AddBlendByBool(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Blend By Int node to a graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param NumPoses - Number of pose inputs (default 2)
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @return Node GUID if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString AddBlendByInt(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		int32 NumPoses = 2,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Layered Blend Per Bone node to a graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @return Node GUID if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString AddLayeredBlend(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Slot node to a graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param SlotName - Name of the animation slot (default "DefaultSlot")
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @return Node GUID if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString AddSlotNode(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& SlotName = TEXT("DefaultSlot"),
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Save Cached Pose node to a graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param CacheName - Name for the cached pose
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @return Node GUID if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString AddSaveCachedPose(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& CacheName,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Use Cached Pose node to a graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param CacheName - Name of the cached pose to use
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @return Node GUID if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString AddUseCachedPose(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& CacheName,
		float PosX = 0.0f,
		float PosY = 0.0f);

	// ============================================================================
	// STATE MACHINE MUTATIONS
	// ============================================================================

	/**
	 * Add a new state to a state machine.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param StateName - Name for the new state
	 * @param PosX - X position in state machine graph
	 * @param PosY - Y position in state machine graph
	 * @return Node GUID if successful
	 *
	 * Example:
	 *   state_id = unreal.AnimGraphService.add_state(
	 *       "/Game/ABP_Character",
	 *       "Locomotion",
	 *       "Idle"
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|StateMachine")
	static FString AddState(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& StateName,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a conduit to a state machine.
	 * Conduits are pass-through nodes that can route transitions based on conditions.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param ConduitName - Name for the new conduit
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @return Node GUID if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|StateMachine")
	static FString AddConduit(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& ConduitName,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a transition between two states.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param SourceStateName - Name of the source state
	 * @param DestStateName - Name of the destination state
	 * @param BlendDuration - Blend duration in seconds (default 0.2)
	 * @return Node GUID if successful
	 *
	 * Example:
	 *   transition_id = unreal.AnimGraphService.add_transition(
	 *       "/Game/ABP_Character",
	 *       "Locomotion",
	 *       "Idle",
	 *       "Walking",
	 *       0.25
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|StateMachine")
	static FString AddTransition(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& SourceStateName,
		const FString& DestStateName,
		float BlendDuration = 0.2f);

	/**
	 * Remove a state from a state machine.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param StateName - Name of the state to remove
	 * @param bRemoveTransitions - Whether to also remove connected transitions (default true)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|StateMachine")
	static bool RemoveState(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& StateName,
		bool bRemoveTransitions = true);

	/**
	 * Remove a transition between two states.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param SourceStateName - Source state
	 * @param DestStateName - Destination state
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|StateMachine")
	static bool RemoveTransition(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& SourceStateName,
		const FString& DestStateName);

	/**
	 * Set which state the state machine's Entry node points to.
	 * Non-destructively re-links the entry: breaks the old entry link and links to the chosen state.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param StateName - Name of the state to make the entry/default state
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.AnimGraphService.set_entry_state("/Game/ABP_Character", "Locomotion", "Idle")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|StateMachine")
	static bool SetEntryState(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& StateName);

	/**
	 * Set the priority order of a transition (lower = higher priority when several rules go true at once).
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param SourceStateName - Source state
	 * @param DestStateName - Destination state
	 * @param Priority - Priority order (default 1; smaller wins)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|StateMachine")
	static bool SetTransitionPriority(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& SourceStateName,
		const FString& DestStateName,
		int32 Priority = 1);

	/**
	 * Set the crossfade/blend settings of a transition.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param SourceStateName - Source state
	 * @param DestStateName - Destination state
	 * @param BlendDuration - Crossfade duration in seconds
	 * @param BlendMode - Alpha blend option: "Linear", "Cubic", "HermiteCubic", "Sinusoidal",
	 *                    "QuadraticInOut", "CubicInOut", "ExpInOut" (default "Linear")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|StateMachine")
	static bool SetTransitionBlend(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& SourceStateName,
		const FString& DestStateName,
		float BlendDuration = 0.2f,
		const FString& BlendMode = TEXT("Linear"));

	// ============================================================================
	// TRANSITION RULES
	// ============================================================================

	/**
	 * Drive a transition's "Can Enter Transition" result from a Blueprint bool variable.
	 * This is what makes a transition actually fire. Without a rule the transition is inert.
	 * Non-destructive: clears any prior rule logic in the transition graph first, then wires the variable.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param SourceStateName - Source state
	 * @param DestStateName - Destination state
	 * @param BoolVariableName - Name of an existing bool member variable on the AnimBP (e.g. "bIsDead")
	 * @param bInvert - If true, transition fires when the variable is FALSE (inserts a NOT)
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.AnimGraphService.set_transition_rule_from_bool(
	 *       "/Game/ABP_Character", "Locomotion", "Idle", "Dead", "bIsDead")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|TransitionRules")
	static bool SetTransitionRuleFromBool(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& SourceStateName,
		const FString& DestStateName,
		const FString& BoolVariableName,
		bool bInvert = false);

	/**
	 * Drive a transition from a numeric comparison against a Blueprint float variable
	 * (e.g. Speed > 150). Non-destructive: clears prior rule logic first.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param SourceStateName - Source state
	 * @param DestStateName - Destination state
	 * @param FloatVariableName - Name of an existing float member variable (e.g. "Speed")
	 * @param Comparison - One of "greater", "less", "greater_equal", "less_equal", "equal", "not_equal"
	 * @param Threshold - The constant to compare against
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.AnimGraphService.set_transition_rule_comparison(
	 *       "/Game/ABP_Character", "Locomotion", "Idle", "Walk", "Speed", "greater", 10.0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|TransitionRules")
	static bool SetTransitionRuleComparison(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& SourceStateName,
		const FString& DestStateName,
		const FString& FloatVariableName,
		const FString& Comparison,
		float Threshold);

	/**
	 * Make a transition fire automatically based on the source state's most relevant asset
	 * player remaining time. Ideal for one-shot animations (attack -> idle, hit-react -> idle).
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param SourceStateName - Source state (must contain a sequence/asset player)
	 * @param DestStateName - Destination state
	 * @param TriggerTime - Seconds before the asset ends to trigger. < 0 means trigger
	 *                      'CrossfadeDuration' seconds before end (default -1.0)
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.AnimGraphService.set_transition_rule_automatic(
	 *       "/Game/ABP_Character", "Combat", "Attack", "Idle")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|TransitionRules")
	static bool SetTransitionRuleAutomatic(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& SourceStateName,
		const FString& DestStateName,
		float TriggerTime = -1.0f);

	/**
	 * Remove the rule logic from a transition (non-destructive reset of the rule graph).
	 * Breaks the link into the result node and clears the automatic flag, leaving the
	 * transition node and its source/dest connections intact.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param SourceStateName - Source state
	 * @param DestStateName - Destination state
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|TransitionRules")
	static bool ClearTransitionRule(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& SourceStateName,
		const FString& DestStateName);

	// ============================================================================
	// HIGH-LEVEL STATE AUTHORING
	// ============================================================================

	/**
	 * Set the animation for a state in one call: ensures a sequence player exists inside the
	 * state's graph, assigns the animation, sets loop/play-rate, and connects it to the state's
	 * Output Pose. Non-destructive: reuses an existing sequence player if the state already has one.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @param StateName - Name of the state
	 * @param AnimSequencePath - Path to the animation sequence asset
	 * @param bLoop - Whether the animation should loop (default true)
	 * @param PlayRate - Playback rate multiplier (default 1.0)
	 * @return Node GUID of the sequence player if successful, empty string otherwise
	 *
	 * Example:
	 *   unreal.AnimGraphService.set_state_animation(
	 *       "/Game/ABP_Character", "Locomotion", "Idle", "/Game/Anims/Idle_Loop", True)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|StateAuthoring")
	static FString SetStateAnimation(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& StateName,
		const FString& AnimSequencePath,
		bool bLoop = true,
		float PlayRate = 1.0f);

	/**
	 * Build (or extend) an entire state machine from a single JSON spec in one atomic transaction.
	 * Idempotent: existing states/transitions are reused, not duplicated. Compiles at the end and
	 * returns a JSON report of what was created and any errors.
	 *
	 * Spec format:
	 * {
	 *   "states": [
	 *     {"name": "Idle",   "animation": "/Game/Anims/Idle",   "loop": true,  "pos": [0, 0]},
	 *     {"name": "Walk",   "animation": "/Game/Anims/Walk",   "loop": true,  "pos": [300, 0]},
	 *     {"name": "Attack", "animation": "/Game/Anims/Attack", "loop": false, "pos": [600, 0]}
	 *   ],
	 *   "transitions": [
	 *     {"from": "Idle",   "to": "Walk",   "rule": {"type": "comparison", "variable": "Speed", "op": "greater", "value": 10}, "blend": 0.2},
	 *     {"from": "Walk",   "to": "Idle",   "rule": {"type": "comparison", "variable": "Speed", "op": "less_equal", "value": 10}},
	 *     {"from": "Idle",   "to": "Attack", "rule": {"type": "bool", "variable": "bAttack"}},
	 *     {"from": "Attack", "to": "Idle",   "rule": {"type": "automatic"}}
	 *   ],
	 *   "entry": "Idle"
	 * }
	 *
	 * rule.type is one of: "bool" (uses "variable", optional "invert"),
	 *   "comparison" (uses "variable", "op", "value"), "automatic" (optional "trigger_time"),
	 *   "always" (always-true), or omit "rule" to leave the transition inert.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine (created if it does not exist)
	 * @param SpecJson - JSON string describing states, transitions and entry
	 * @param PosX - X position of the state machine node in the AnimGraph if it must be created
	 * @param PosY - Y position of the state machine node in the AnimGraph if it must be created
	 * @return JSON report string: {"success", "machine", "states_created", "transitions_created", "errors": [...]}
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|StateAuthoring")
	static FString BuildStateMachine(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& SpecJson,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Validate a state machine for the most common authoring mistakes and return a structured report.
	 * Flags: no entry state, inert transitions (rule graph drives nothing -> never fires),
	 * states with no animation wired to Output Pose, and unreachable states.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param StateMachineName - Name of the state machine
	 * @return Validation result with errors/warnings
	 *
	 * Example:
	 *   result = unreal.AnimGraphService.validate_state_machine("/Game/ABP_Character", "Locomotion")
	 *   if not result.is_valid:
	 *       for err in result.errors: print(err)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|StateAuthoring")
	static FAnimStateMachineValidationResult ValidateStateMachine(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName);

	// ============================================================================
	// ANIMGRAPH CONNECTIONS
	// ============================================================================

	/**
	 * Connect two animation nodes via their pose pins.
	 * This is the AnimGraph equivalent of connecting exec/data pins in EventGraph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param SourceNodeId - GUID of the source node
	 * @param SourcePinName - Name of the output pose pin (default "Pose")
	 * @param TargetNodeId - GUID of the target node
	 * @param TargetPinName - Name of the input pose pin (default "Pose")
	 * @return True if successful
	 *
	 * Example:
	 *   # Connect sequence player to output pose
	 *   unreal.AnimGraphService.connect_anim_nodes(
	 *       "/Game/ABP_Character",
	 *       "AnimGraph",
	 *       seq_player_id,
	 *       "Pose",
	 *       output_pose_id,
	 *       "Result"
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Connections")
	static bool ConnectAnimNodes(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& SourceNodeId,
		const FString& SourcePinName = TEXT("Pose"),
		const FString& TargetNodeId = TEXT(""),
		const FString& TargetPinName = TEXT("Result"));

	/**
	 * Connect a node directly to the Output Pose (result node) in a graph.
	 * Convenience method for the most common connection pattern.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param SourceNodeId - GUID of the node to connect
	 * @param SourcePinName - Name of the output pin on source node
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Connections")
	static bool ConnectToOutputPose(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& SourceNodeId,
		const FString& SourcePinName = TEXT("Pose"));

	/**
	 * Disconnect a node's output from all connections.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the node
	 * @param PinName - Name of the pin to disconnect
	 * @return True if any connections were broken
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Connections")
	static bool DisconnectAnimNode(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		const FString& PinName);

	/**
	 * Get the Output Pose (result) node ID for a graph.
	 * Useful for connecting to the final output.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @return Node GUID of the output pose node, or empty string
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Connections")
	static FString GetOutputPoseNodeId(
		const FString& AnimBlueprintPath,
		const FString& GraphName);

	// ============================================================================
	// ANIMATION ASSET ASSIGNMENT
	// ============================================================================

	/**
	 * Set the animation sequence on a sequence player node.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph containing the node
	 * @param NodeId - GUID of the sequence player node
	 * @param AnimSequencePath - Path to the animation sequence asset
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.AnimGraphService.set_sequence_player_asset(
	 *       "/Game/ABP_Character",
	 *       "IdleState",
	 *       node_id,
	 *       "/Game/Animations/Idle_Loop"
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Assets")
	static bool SetSequencePlayerAsset(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		const FString& AnimSequencePath);

	/**
	 * Set the blend space on a blend space player node.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the blend space player node
	 * @param BlendSpacePath - Path to the blend space asset
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Assets")
	static bool SetBlendSpaceAsset(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		const FString& BlendSpacePath);

	/**
	 * Get the current animation asset path from a player node.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param NodeId - GUID of the player node
	 * @return Asset path, or empty string if not set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Assets")
	static FString GetNodeAnimationAsset(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& NodeId);

	// ============================================================================
	// ADVANCED ANIMATION NODES
	// ============================================================================

	/**
	 * Add a Two Bone IK node to a graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @return Node GUID if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString AddTwoBoneIKNode(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Modify Bone (Transform Bone) node to a graph.
	 *
	 * @param AnimBlueprintPath - Full path to the Animation Blueprint
	 * @param GraphName - Name of the graph
	 * @param BoneName - Name of the bone to modify
	 * @param PosX - X position
	 * @param PosY - Y position
	 * @return Node GUID if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString AddModifyBoneNode(
		const FString& AnimBlueprintPath,
		const FString& GraphName,
		const FString& BoneName = TEXT(""),
		float PosX = 0.0f,
		float PosY = 0.0f);

private:
	/** Load an Animation Blueprint from path */
	static class UAnimBlueprint* LoadAnimBlueprint(const FString& AnimBlueprintPath);

	/** Find a graph by name in an Animation Blueprint */
	static class UEdGraph* FindAnimGraph(class UAnimBlueprint* AnimBlueprint, const FString& GraphName);

	/** Find a state machine node by name */
	static class UAnimGraphNode_StateMachine* FindStateMachineNode(class UAnimBlueprint* AnimBlueprint, const FString& MachineName);

	/** Find a state node within a state machine graph */
	static class UAnimStateNodeBase* FindStateNode(class UEdGraph* StateMachineGraph, const FString& StateName);

	/** Find a transition node between two states */
	static class UAnimStateTransitionNode* FindTransitionNode(class UEdGraph* StateMachineGraph, const FString& SourceState, const FString& DestState);

	/** Get the editor for an Animation Blueprint, opening it if necessary */
	static class IAnimationBlueprintEditor* GetAnimBlueprintEditor(class UAnimBlueprint* AnimBlueprint);

	/** Resolve blueprint + state machine graph + transition node in one step. Returns null on any failure. */
	static class UAnimStateTransitionNode* ResolveTransition(
		const FString& AnimBlueprintPath,
		const FString& StateMachineName,
		const FString& SourceStateName,
		const FString& DestStateName,
		class UAnimBlueprint*& OutBlueprint);

	/** Get the transition result node ("Can Enter Transition") for a transition's rule graph. */
	static class UAnimGraphNode_TransitionResult* GetTransitionResultNode(class UAnimStateTransitionNode* Transition);

	/** Non-destructively clear a transition's rule graph: break the result input link, remove rule logic nodes, reset automatic flag. */
	static void ResetTransitionRule(class UAnimStateTransitionNode* Transition);

	/** Find the sequence/asset player node inside a state's graph that feeds (directly) the Output Pose, if any. */
	static class UAnimGraphNode_Base* FindStateAssetPlayer(class UAnimStateNodeBase* StateNode);

	/** Describe the rule driving a transition (for introspection). Fills RuleType/RuleVariable/RuleSummary/bHasRule. */
	static void DescribeTransitionRule(class UAnimStateTransitionNode* Transition, FAnimTransitionInfo& OutInfo);
};
