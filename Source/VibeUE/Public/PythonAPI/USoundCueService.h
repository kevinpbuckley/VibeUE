// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "USoundCueService.generated.h"

// ============================================================================
// RESULT / INFO STRUCTS
// ============================================================================

/** Result returned by mutating SoundCue operations */
USTRUCT(BlueprintType)
struct FSoundCueResult
{
	GENERATED_BODY()

	/** Whether the operation succeeded */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	bool bSuccess = false;

	/** Human-readable result message */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	FString Message;

	/** Asset path of the affected asset (empty on failure) */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	FString AssetPath;
};

/**
 * Information about a single node inside a SoundCue graph.
 * NodeIndex is stable within a single call session but should be refreshed via
 * list_nodes() after any structural mutation.
 */
USTRUCT(BlueprintType)
struct FSoundCueNodeInfo
{
	GENERATED_BODY()

	/** 0-based index into the non-root node list returned by list_nodes() */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	int32 NodeIndex = -1;

	/** UClass name, e.g. "SoundNodeWavePlayer", "SoundNodeRandom" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	FString NodeClass;

	/** Editor display title */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	FString NodeTitle;

	/**
	 * Indices of nodes that feed INTO this node (i.e. entries in ChildNodes).
	 * -1 entries indicate an unconnected input slot.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	TArray<int32> ChildIndices;

	/** X position in the SoundCue editor graph */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	float PosX = 0.0f;

	/** Y position in the SoundCue editor graph */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	float PosY = 0.0f;

	/** True if this node is currently set as FirstNode (the cue output) */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	bool bIsRootNode = false;
};

/** Summary information about a SoundCue asset */
USTRUCT(BlueprintType)
struct FSoundCueInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	float VolumeMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	float PitchMultiplier = 1.0f;

	/** Name of the assigned SoundClass, or empty if none */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	FString SoundClassName;

	/** Asset path of the assigned attenuation settings asset, or empty if none */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	FString AttenuationPath;

	/** Total number of sound nodes in the graph (excludes the output node) */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	int32 NodeCount = 0;

	/** Index (in list_nodes) of the node set as FirstNode, or -1 if none */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	int32 RootNodeIndex = -1;
};

/** Summary information about a SoundWave asset */
USTRUCT(BlueprintType)
struct FSoundWaveInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	FString AssetPath;

	/** Total playback duration in seconds (0 if unknown/streaming) */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	float Duration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	int32 SampleRate = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	int32 NumChannels = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	bool bLooping = false;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio")
	bool bStreaming = false;
};

// ============================================================================
// SERVICE CLASS
// ============================================================================

/**
 * SoundCue authoring service exposed directly to Python.
 *
 * Provides creation, inspection, and editing of SoundCue assets including
 * their node graphs, attenuation settings, and SoundWave references.
 *
 * Node indexing: list_nodes() returns nodes in graph order (0-based). Indices
 * are valid until the next structural mutation, so always re-call list_nodes()
 * after add/connect/remove operations before referencing indices again.
 *
 * Audio flow in SoundCue graphs: audio travels FROM leaf nodes TOWARD the root.
 * WavePlayer nodes produce audio, which feeds UP into Mixer/Random/etc., which
 * feeds into the cue's root (FirstNode). connect_nodes(parent, child, slot)
 * means "child provides audio TO parent at input slot".
 *
 * Python Usage:
 *   import unreal
 *
 *   # Create a simple cue with a single wave
 *   result = unreal.SoundCueService.create_sound_cue(
 *       "/Game/Audio/SC_Footstep",
 *       "/Game/Audio/SW_Step_Stone"
 *   )
 *
 *   # Inspect nodes
 *   nodes = unreal.SoundCueService.list_nodes("/Game/Audio/SC_Footstep")
 *   for n in nodes:
 *       print(f"[{n.node_index}] {n.node_class} — children: {n.child_indices}")
 *
 *   # Build a randomised footstep cue manually
 *   r = unreal.SoundCueService.add_random_node("/Game/Audio/SC_Footstep", -200, 0)
 *   wp1 = unreal.SoundCueService.add_wave_player_node(
 *       "/Game/Audio/SC_Footstep", "/Game/Audio/SW_Step_Stone", -400, -100)
 *   wp2 = unreal.SoundCueService.add_wave_player_node(
 *       "/Game/Audio/SC_Footstep", "/Game/Audio/SW_Step_Dirt", -400, 100)
 *   nodes = unreal.SoundCueService.list_nodes("/Game/Audio/SC_Footstep")
 *   # ... connect and set root
 *
 * @note All methods are static
 * @note Always call save_sound_cue() after editing to persist changes
 *
 * **C++ Source:**
 *
 * - **Plugin**: VibeUE
 * - **Module**: VibeUE
 * - **File**: USoundCueService.h
 */
UCLASS(BlueprintType)
class VIBEUE_API USoundCueService : public UObject
{
	GENERATED_BODY()

public:

	// ============================================================================
	// ASSET LIFECYCLE
	// ============================================================================

	/**
	 * Create a new SoundCue asset at the given content-browser path.
	 *
	 * @param AssetPath   - Full asset path including name, e.g. "/Game/Audio/SC_Footstep"
	 * @param SoundWavePath - Optional: path to a SoundWave to add as the initial WavePlayer node.
	 *                        Pass empty string to create an empty cue.
	 * @return FSoundCueResult with bSuccess=true and AssetPath set on success
	 *
	 * Example:
	 *   result = unreal.SoundCueService.create_sound_cue("/Game/Audio/SC_Footstep", "/Game/Audio/SW_Step")
	 *   if result.b_success:
	 *       print(f"Created: {result.asset_path}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Lifecycle")
	static FSoundCueResult CreateSoundCue(
		const FString& AssetPath,
		const FString& SoundWavePath = TEXT(""));

	/**
	 * Get summary information about a SoundCue asset.
	 *
	 * @param AssetPath - Full path to the SoundCue asset
	 * @return FSoundCueInfo; AssetPath will be empty on failure
	 *
	 * Example:
	 *   info = unreal.SoundCueService.get_sound_cue_info("/Game/Audio/SC_Footstep")
	 *   print(f"Nodes: {info.node_count}, Volume: {info.volume_multiplier}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Lifecycle")
	static FSoundCueInfo GetSoundCueInfo(const FString& AssetPath);

	/**
	 * Save a SoundCue asset to disk.
	 * Always call this after editing to persist changes.
	 *
	 * @param AssetPath - Full path to the SoundCue asset
	 * @return True if saved successfully
	 *
	 * Example:
	 *   unreal.SoundCueService.save_sound_cue("/Game/Audio/SC_Footstep")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Lifecycle")
	static bool SaveSoundCue(const FString& AssetPath);

	// ============================================================================
	// NODE INSPECTION
	// ============================================================================

	/**
	 * List all sound nodes in a SoundCue graph.
	 *
	 * Returns all USoundNode-derived nodes (excluding the invisible output node).
	 * Node indices in the returned array are the indices used by connect_nodes,
	 * set_root_node, set_wave_player_asset, etc.
	 *
	 * Call this after any structural change to get fresh indices.
	 *
	 * @param AssetPath - Full path to the SoundCue asset
	 * @return Array of node information, empty on failure
	 *
	 * Example:
	 *   nodes = unreal.SoundCueService.list_nodes("/Game/Audio/SC_Footstep")
	 *   for n in nodes:
	 *       root_flag = " [ROOT]" if n.b_is_root_node else ""
	 *       print(f"[{n.node_index}] {n.node_class}{root_flag} children={n.child_indices}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Inspection")
	static TArray<FSoundCueNodeInfo> ListNodes(const FString& AssetPath);

	// ============================================================================
	// NODE CREATION
	// ============================================================================

	/**
	 * Add a Wave Player node that references a SoundWave.
	 *
	 * @param AssetPath      - Full path to the SoundCue asset
	 * @param SoundWavePath  - Full path to the SoundWave asset to assign (can be empty)
	 * @param PosX           - X position in the graph editor
	 * @param PosY           - Y position in the graph editor
	 * @return FSoundCueResult; Message contains the new node index on success
	 *
	 * Example:
	 *   result = unreal.SoundCueService.add_wave_player_node(
	 *       "/Game/Audio/SC_Footstep", "/Game/Audio/SW_Step_Stone", -400, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Nodes")
	static FSoundCueResult AddWavePlayerNode(
		const FString& AssetPath,
		const FString& SoundWavePath = TEXT(""),
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Random node that picks one of its inputs randomly each time it plays.
	 *
	 * @param AssetPath - Full path to the SoundCue asset
	 * @param PosX      - X position in the graph editor
	 * @param PosY      - Y position in the graph editor
	 * @return FSoundCueResult; Message contains the new node index on success
	 *
	 * Example:
	 *   result = unreal.SoundCueService.add_random_node("/Game/Audio/SC_Footstep", -200, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Nodes")
	static FSoundCueResult AddRandomNode(
		const FString& AssetPath,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Mixer node that blends multiple inputs simultaneously.
	 *
	 * @param AssetPath  - Full path to the SoundCue asset
	 * @param NumInputs  - Number of initial input slots (1–32, default 2)
	 * @param PosX       - X position in the graph editor
	 * @param PosY       - Y position in the graph editor
	 * @return FSoundCueResult; Message contains the new node index on success
	 *
	 * Example:
	 *   result = unreal.SoundCueService.add_mixer_node("/Game/Audio/SC_Ambience", 3, -200, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Nodes")
	static FSoundCueResult AddMixerNode(
		const FString& AssetPath,
		int32 NumInputs = 2,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Modulator node that randomly varies pitch and volume each play.
	 *
	 * @param AssetPath - Full path to the SoundCue asset
	 * @param PosX      - X position in the graph editor
	 * @param PosY      - Y position in the graph editor
	 * @return FSoundCueResult; Message contains the new node index on success
	 *
	 * Example:
	 *   result = unreal.SoundCueService.add_modulator_node("/Game/Audio/SC_Footstep", -100, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Nodes")
	static FSoundCueResult AddModulatorNode(
		const FString& AssetPath,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add an Attenuation node for 3D distance-based volume falloff.
	 *
	 * @param AssetPath - Full path to the SoundCue asset
	 * @param PosX      - X position in the graph editor
	 * @param PosY      - Y position in the graph editor
	 * @return FSoundCueResult; Message contains the new node index on success
	 *
	 * Example:
	 *   result = unreal.SoundCueService.add_attenuation_node("/Game/Audio/SC_Footstep", -50, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Nodes")
	static FSoundCueResult AddAttenuationNode(
		const FString& AssetPath,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Looping node that loops its input.
	 *
	 * @param AssetPath - Full path to the SoundCue asset
	 * @param PosX      - X position in the graph editor
	 * @param PosY      - Y position in the graph editor
	 * @return FSoundCueResult; Message contains the new node index on success
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Nodes")
	static FSoundCueResult AddLoopingNode(
		const FString& AssetPath,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Concatenator node that plays its inputs sequentially.
	 *
	 * @param AssetPath  - Full path to the SoundCue asset
	 * @param NumInputs  - Number of initial input slots (2–32, default 2)
	 * @param PosX       - X position in the graph editor
	 * @param PosY       - Y position in the graph editor
	 * @return FSoundCueResult; Message contains the new node index on success
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Nodes")
	static FSoundCueResult AddConcatenatorNode(
		const FString& AssetPath,
		int32 NumInputs = 2,
		float PosX = 0.0f,
		float PosY = 0.0f);

	/**
	 * Add a Delay node that introduces a pause before playing its input.
	 *
	 * @param AssetPath - Full path to the SoundCue asset
	 * @param PosX      - X position in the graph editor
	 * @param PosY      - Y position in the graph editor
	 * @return FSoundCueResult; Message contains the new node index on success
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Nodes")
	static FSoundCueResult AddDelayNode(
		const FString& AssetPath,
		float PosX = 0.0f,
		float PosY = 0.0f);

	// ============================================================================
	// NODE CONNECTIONS
	// ============================================================================

	/**
	 * Connect two nodes: wire ChildNode into ParentNode's input slot.
	 *
	 * Audio flows: ChildNode → ParentNode
	 * (ChildNode produces audio that ParentNode consumes at InputSlot)
	 *
	 * @param AssetPath   - Full path to the SoundCue asset
	 * @param ParentIndex - Index of the consuming node (from list_nodes)
	 * @param ChildIndex  - Index of the producing node (from list_nodes)
	 * @param InputSlot   - Zero-based input slot index on the parent node
	 * @return True if connected successfully
	 *
	 * Example:
	 *   # Connect WavePlayer (index 1) into Random node (index 0) at slot 0
	 *   unreal.SoundCueService.connect_nodes("/Game/Audio/SC_Footstep", 0, 1, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Connections")
	static bool ConnectNodes(
		const FString& AssetPath,
		int32 ParentIndex,
		int32 ChildIndex,
		int32 InputSlot);

	/**
	 * Set the root (output) node of the SoundCue — the last node in the audio chain.
	 *
	 * The root node is what the SoundCue plays when triggered.
	 * In a simple cue: WavePlayer IS the root.
	 * In a complex cue: Random/Mixer/Modulator chain → root.
	 *
	 * @param AssetPath - Full path to the SoundCue asset
	 * @param NodeIndex - Index of the node to set as root (from list_nodes)
	 * @return True if successful
	 *
	 * Example:
	 *   # Set node 0 (a Random node) as the cue output
	 *   unreal.SoundCueService.set_root_node("/Game/Audio/SC_Footstep", 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Connections")
	static bool SetRootNode(
		const FString& AssetPath,
		int32 NodeIndex);

	// ============================================================================
	// NODE PROPERTIES
	// ============================================================================

	/**
	 * Assign a SoundWave to an existing Wave Player node.
	 *
	 * @param AssetPath     - Full path to the SoundCue asset
	 * @param NodeIndex     - Index of the WavePlayer node (from list_nodes)
	 * @param SoundWavePath - Full path to the SoundWave asset
	 * @return True if successful; False if node is not a WavePlayer or not found
	 *
	 * Example:
	 *   unreal.SoundCueService.set_wave_player_asset(
	 *       "/Game/Audio/SC_Footstep", 1, "/Game/Audio/SW_Step_Gravel")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|NodeProperties")
	static bool SetWavePlayerAsset(
		const FString& AssetPath,
		int32 NodeIndex,
		const FString& SoundWavePath);

	// ============================================================================
	// CUE SETTINGS
	// ============================================================================

	/**
	 * Set the volume multiplier on a SoundCue.
	 *
	 * @param AssetPath - Full path to the SoundCue asset
	 * @param Value     - Volume multiplier (default 1.0)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Settings")
	static bool SetVolumeMultiplier(const FString& AssetPath, float Value);

	/**
	 * Set the pitch multiplier on a SoundCue.
	 *
	 * @param AssetPath - Full path to the SoundCue asset
	 * @param Value     - Pitch multiplier (default 1.0)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Settings")
	static bool SetPitchMultiplier(const FString& AssetPath, float Value);

	/**
	 * Assign a SoundClass to a SoundCue.
	 *
	 * @param AssetPath      - Full path to the SoundCue asset
	 * @param SoundClassPath - Full path to the SoundClass asset
	 *                         (e.g. "/Game/Audio/SC_SFX" or "/Engine/EngineSounds/Master")
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.SoundCueService.set_sound_class(
	 *       "/Game/Audio/SC_Footstep", "/Game/Audio/Classes/SFX")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|Settings")
	static bool SetSoundClass(const FString& AssetPath, const FString& SoundClassPath);

	// ============================================================================
	// SOUNDWAVE UTILITIES
	// ============================================================================

	/**
	 * Get information about a SoundWave asset.
	 *
	 * @param SoundWavePath - Full path to the SoundWave asset
	 * @return FSoundWaveInfo; AssetPath will be empty on failure
	 *
	 * Example:
	 *   info = unreal.SoundCueService.get_sound_wave_info("/Game/Audio/SW_Step_Stone")
	 *   print(f"Duration: {info.duration}s, Channels: {info.num_channels}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Audio|SoundWave")
	static FSoundWaveInfo GetSoundWaveInfo(const FString& SoundWavePath);

private:

	/** Load a SoundCue from a content-browser path. Returns nullptr on failure. */
	static class USoundCue* LoadSoundCue(const FString& AssetPath);

	/** Load a SoundWave from a content-browser path. Returns nullptr on failure. */
	static class USoundWave* LoadSoundWave(const FString& AssetPath);

	/**
	 * Collect all USoundCueGraphNode instances from the cue's graph, in order.
	 * Excludes the invisible USoundCueGraphNode_Root output node.
	 * The returned array index corresponds to the NodeIndex used throughout this service.
	 */
	static TArray<class USoundCueGraphNode*> GetGraphNodes(class USoundCue* SoundCue);

	/** Get a specific graph node by index. Returns nullptr if out of range. */
	static class USoundCueGraphNode* GetGraphNodeAtIndex(class USoundCue* SoundCue, int32 Index);
};
