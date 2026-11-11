// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "Engine/Blueprint.h"
#include "UBlueprintService.generated.h"


/**
 * Information about a single UEdGraph attached to a Blueprint.
 * Returned by ListGraphs — one entry per graph object, including interface,
 * nested and extension graphs. Distinct objects may have the same GraphName;
 * name-keyed consumers must reject ambiguity instead of overwriting entries.
 */
USTRUCT(BlueprintType)
struct FBlueprintGraphInfo
{
	GENERATED_BODY()

	/** Tab name as shown in the My Blueprint panel (e.g. "EventGraph", "Graph_PlayerInput", "MyFunction") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphName;

	/** "Ubergraph" | "Function" | "Macro" | "DelegateSignature" | "Interface (<name>)" | "Subgraph" | "Other" */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphKind;

	/** Number of nodes in this graph (cheap to compute, useful as a sanity signal) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 NodeCount = 0;

	/** Full object path of the graph (Graph->GetPathName()) — unique even when several graphs
	 *  share a GraphName. Pass it back to any GraphName parameter to disambiguate duplicates. */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphPath;
};




/**
 * Information about a blueprint function parameter
 */
USTRUCT(BlueprintType)
struct FBlueprintFunctionParameterInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ParameterName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ParameterType;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsOutput = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsReference = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;
};

/**
 * Information about a single Timeline on a Blueprint. Returned by get_timelines.
 * (Replaces the earlier FBlueprintFunctionParameterInfo overload that carried the timeline
 * name in parameter_name — read the fields below by name instead.)
 */
USTRUCT(BlueprintType)
struct FBlueprintTimelineInfo
{
	GENERATED_BODY()

	/** Timeline (component variable) name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TimelineName;

	/** Total number of tracks (float + vector + color + event) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 TrackCount = 0;

	/** Timeline length in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	float Length = 0.0f;

	/** Whether the timeline loops */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bLoop = false;

	/** Whether the timeline auto-plays */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bAutoPlay = false;
};









// ============================================================================
// BATCH GRAPH BUILDER TYPES
//
// Legacy descriptor types. The batch graph builder they fed is gone; author graphs with
// BlueprintToolsets.BlueprintDslToolset (Read / Write / Edit) or node-by-node with
// BlueprintToolsets.BlueprintToolset (CreateNode / ConnectPins / SetPinValue). Kept because the
// node-creation helper inside this service still takes FGraphNodeDesc.
// ============================================================================

/**
 * Describes a single node to create.
 * Each node has a local ref (for wiring) and a type that selects a creation path.
 */
USTRUCT(BlueprintType)
struct FGraphNodeDesc
{
	GENERATED_BODY()

	/** Local reference for wiring (e.g. "A", "BeginPlay", any unique string) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Ref;

	/**
	 * Node type — determines which creation method is used:
	 *   function_call, spawner_key, variable_get, variable_set,
	 *   event, custom_event, branch, cast, print_string,
	 *   input_action, math, comparison, delegate_bind,
	 *   create_event, validated_get, member_get, create_delegate
	 *
	 * The pre-existing function terminals are addressed as EXISTING nodes rather than
	 * created: type "function_entry" (ref "entry") and "function_result" (ref "result",
	 * "result_2", ...), each carrying params["existing"]="true", so a descriptor set for a
	 * function graph can carry its parameter/return wiring without respawning the terminals.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Type;

	/**
	 * Type-specific parameters (e.g. "class":"KismetMathLibrary", "function":"Clamp").
	 * Any node type also accepts an optional "group":"<title>" layout hint — each distinct title
	 * becomes a comment box wrapping its member nodes.
	 * "existing":"true" marks a descriptor that resolves to a node already in the graph
	 * (used for function_entry/function_result) rather than creating a new one.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TMap<FString, FString> Params;
};




/**
 * Blueprint Service - Timelines, macro graphs, a layout audit and an editor refresh. That is all
 * that is left; the engine's toolsets own the rest of the Blueprint surface.
 *
 * Cut to the delta on the 5-9 branch. Use the engine for everything else:
 *  - whole-graph authoring (VibeUE's own batch graph builder is gone):
 *    `BlueprintToolsets.BlueprintDslToolset` (Read / Write / Edit / Docs / Schema / Diff /
 *    FindNodeReferences) -- a graph is canonical text, applied transactionally against the
 *    revision token the previous Read returned
 *  - per-node graph work and graph reads: `BlueprintToolsets.BlueprintToolset`
 *    (ListGraphs / GetGraph / FindNodes / GetNodeInfos / GetConnectedSubgraph / FindNodeTypes /
 *    CreateNode / DeleteNode / ConnectPins / DisconnectPins / SetPinValue / SetNodePosition /
 *    ArrangeGraphLayered / ArrangeNodes / AddCommentBox / DiffGraphs / CompileBlueprint)
 *  - the class itself, same toolset: Create, GetParent / SetParent, GetDefaultObject,
 *    ListVariables / AddVariable / RemoveVariable (+ the per-variable flag getters and setters),
 *    ListFunctions / ListEvents, AddFunctionGraph / AddEvent -- either one, named after an
 *    inherited member, *is* the override, so it covers both listing and taking an override --
 *    AddEventDispatcher / ListEventDispatchers, AddInterface / ListInterfaces
 *  - components on a Blueprint: `EditorToolset.ActorToolset`. AddComponent takes the Blueprint
 *    asset itself as Owner; GetComponents enumerates the SCS when handed the Blueprint's CDO
 *    (from BlueprintToolset.GetDefaultObject); RemoveComponent; and SetParentComponent promotes a
 *    component to the scene root, replacing the current one
 *  - what the user is looking at right now: `EditorToolset.EditorAppToolset`
 *    (GetAssetEditorSelection -> focused graph + selected nodes, OpenEditorForAsset)
 *  - any property on any object, Timeline component settings included:
 *    `EditorToolset.ObjectToolset` (GetProperties / SetProperties / ListProperties /
 *    GetPropertySchema)
 *  - find, save and validate assets: `EditorToolset.AssetToolset`
 *    (FindAssets / Exists / SaveAsset / ValidateAssets)
 *
 * What is left is what none of those can do: Timelines (no engine tool touches a
 * UTimelineTemplate), macro-graph creation, a read-only measurement of a graph's layout quality,
 * and forcing an open Blueprint editor to redraw.
 *
 * Every BlueprintPath parameter below also accepts a map/world path and resolves it to that
 * level's Level Blueprint. Every entry point refuses while a Play-In-Editor session is running
 * (logged with a "PIE_ACTIVE:" prefix) -- stop PIE before editing a Blueprint.
 */
UCLASS(BlueprintType)
class VIBEUE_API UBlueprintService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// ============================================================================
	// MACRO GRAPHS
	// ============================================================================

	/**
	 * Add a macro graph to a Blueprint (typically a Macro Library Blueprint).
	 *
	 * The engine has no tool for this: BlueprintToolset.AddFunctionGraph makes function graphs, and
	 * BlueprintDslToolset refuses a macro library outright ("macro graphs have no Graph DSL
	 * representation"). Placing an *instance* of a macro is the DSL's job, by owner path --
	 * (`/Game/MyMacroLib.MyMacroName`) or (`self.MyMacroName`).
	 *
	 * Create the macro graph here, then reference it with the full path format:
	 *   "/Game/MyMacroLib.MyMacroLib:MyMacroName"
	 *
	 * To create a Macro Library Blueprint from Python:
	 *   factory = unreal.BlueprintMacroFactory()
	 *   factory.set_editor_property("parent_class", unreal.Actor.static_class())
	 *   tools = unreal.AssetToolsHelpers.get_asset_tools()
	 *   tools.create_asset("BPMacroLib", "/Game/Macros", unreal.Blueprint, factory)
	 *
	 * Note: a Macro Library holds macro graphs only — adding a *function* graph to one asserts in
	 * the K2 schema.
	 *
	 * @param BlueprintPath - Full path to the target Blueprint
	 * @param MacroName     - Name for the new macro graph
	 * @return True if successful (idempotent — returns true if the macro already exists)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool CreateMacroGraph(
		const FString& BlueprintPath,
		const FString& MacroName
	);

	// ============================================================================
	// TIMELINES
	//
	// No engine toolset tool touches a UTimelineTemplate, so the whole timeline surface stays here.
	// Component-level settings on the spawned UTimelineComponent are still ordinary properties:
	// EditorToolset.ObjectToolset.SetProperties.
	// ============================================================================

	/**
	 * Add a Timeline node to a graph (and the backing UTimelineTemplate on the blueprint).
	 * Equivalent to the "Add Timeline..." action. The new node has exec inputs
	 * (Play, PlayFromStart, Stop, Reverse, ReverseFromEnd, SetNewTime / NewTime),
	 * exec outputs (Update, Finished), and a Direction output; track output pins are
	 * added by add_timeline_float_track / etc.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param GraphName - Name of the graph to place the node in (e.g. "EventGraph")
	 * @param TimelineName - Name for the timeline (also the component variable name). Must be unique.
	 * @param Length - Timeline length in seconds (used when LengthMode is fixed-length)
	 * @param bUseLastKeyFrame - If true, the timeline auto-sizes to the last keyframe instead of using Length
	 * @param bAutoPlay - Whether the timeline auto-plays
	 * @param bLoop - Whether the timeline loops
	 * @param PosX - X position in the graph
	 * @param PosY - Y position in the graph
	 * @param bReplaceExisting - When a UTimelineTemplate with this name already exists (a deleted
	 *                           Timeline node can leave its template behind), true removes it first
	 *                           via remove_timeline and re-adds; false (default) refuses and returns
	 *                           an "ERROR: ..." sentinel string naming the timeline.
	 * @return Node ID (GUID) of the Timeline node on success; empty string on load/graph failure;
	 *         a string starting with "ERROR:" when the name is taken and bReplaceExisting is false
	 *
	 * Example:
	 *   node_id = unreal.BlueprintService.add_timeline("/Game/StateTree/BP_Cube", "EventGraph", "LookAtTimeline", 0.5)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static FString AddTimeline(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& TimelineName,
		float Length = 5.0f,
		bool bUseLastKeyFrame = false,
		bool bAutoPlay = false,
		bool bLoop = false,
		float PosX = 0.0f,
		float PosY = 0.0f,
		bool bReplaceExisting = false
	);

	/**
	 * Add a float interpolation track to an existing timeline. This creates an internal
	 * UCurveFloat for the track and adds a matching float output pin to the Timeline node.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline (as passed to add_timeline)
	 * @param TrackName - Name of the new track (also the name of the output pin on the node)
	 * @return True if the track was added
	 *
	 * Example:
	 *   unreal.BlueprintService.add_timeline_float_track("/Game/StateTree/BP_Cube", "LookAtTimeline", "Alpha")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool AddTimelineFloatTrack(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& TrackName
	);

	/**
	 * Add (or replace) a keyframe on a timeline float track's curve.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline
	 * @param TrackName - Name of the float track
	 * @param Time - Key time in seconds
	 * @param Value - Key value
	 * @param InterpMode - One of "Auto" (cubic, auto tangents — smooth), "Linear", "Constant", "CubicAuto", "CubicUser". Default "Auto".
	 * @return True if the key was added
	 *
	 * Example:
	 *   unreal.BlueprintService.add_timeline_float_key("/Game/StateTree/BP_Cube", "LookAtTimeline", "Alpha", 0.0, 0.0)
	 *   unreal.BlueprintService.add_timeline_float_key("/Game/StateTree/BP_Cube", "LookAtTimeline", "Alpha", 0.5, 1.0)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool AddTimelineFloatKey(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& TrackName,
		float Time,
		float Value,
		const FString& InterpMode = TEXT("Auto")
	);

	/**
	 * List the timelines on a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @return Array of FBlueprintTimelineInfo (timeline_name, track_count, length, loop, auto_play)
	 *
	 * Example:
	 *   for t in unreal.BlueprintService.get_timelines("/Game/StateTree/BP_Cube"):
	 *       print(t.timeline_name, "tracks:", t.track_count, "len:", t.length)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static TArray<FBlueprintTimelineInfo> GetTimelines(
		const FString& BlueprintPath
	);

	/**
	 * Modify settings on an existing timeline. Pass a sentinel to leave a setting unchanged:
	 * empty string for NewName, a negative number for Length, and -1 for the int flags
	 * (0 = false, 1 = true).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Current name of the timeline
	 * @param NewName - New name (also renames the component variable & event-track functions), or "" to leave
	 * @param Length - New fixed length in seconds, or < 0 to leave
	 * @param UseLastKeyFrame - 1 = length follows last keyframe, 0 = fixed length, -1 = leave
	 * @param AutoPlay - 1/0/-1
	 * @param Loop - 1/0/-1
	 * @param Replicated - 1/0/-1
	 * @param IgnoreTimeDilation - 1/0/-1
	 * @return True if the timeline was found and (any) change applied
	 *
	 * Example:
	 *   unreal.BlueprintService.modify_timeline("/Game/StateTree/BP_Cube", "LookAtTimeline", "", 0.75, -1, -1, -1, -1, -1)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool ModifyTimeline(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& NewName = TEXT(""),
		float Length = -1.0f,
		int32 UseLastKeyFrame = -1,
		int32 AutoPlay = -1,
		int32 Loop = -1,
		int32 Replicated = -1,
		int32 IgnoreTimeDilation = -1
	);

	/**
	 * Remove a timeline from a blueprint — deletes the Timeline node and its UTimelineTemplate.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline to remove
	 * @return True if the timeline existed and was removed
	 *
	 * Example:
	 *   unreal.BlueprintService.remove_timeline("/Game/StateTree/BP_Cube", "LookAtTimeline")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool RemoveTimeline(
		const FString& BlueprintPath,
		const FString& TimelineName
	);

	/**
	 * Add a vector interpolation track (3-component UCurveVector) to a timeline.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline
	 * @param TrackName - Name of the new track (also the output pin name on the node)
	 * @return True if the track was added
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool AddTimelineVectorTrack(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& TrackName
	);

	/**
	 * Add a linear-color interpolation track (4-component UCurveLinearColor) to a timeline.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline
	 * @param TrackName - Name of the new track (also the output pin name on the node)
	 * @return True if the track was added
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool AddTimelineColorTrack(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& TrackName
	);

	/**
	 * Add an event track to a timeline. Event tracks add a named exec output pin on the Timeline
	 * node that fires when playback crosses one of the track's keys.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline
	 * @param TrackName - Name of the new event track (also the exec output pin name)
	 * @return True if the track was added
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool AddTimelineEventTrack(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& TrackName
	);

	/**
	 * Remove a track of any type from a timeline (and its output pin on the node).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline
	 * @param TrackName - Name of the track to remove
	 * @return True if the track existed and was removed
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool RemoveTimelineTrack(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& TrackName
	);

	/**
	 * Rename a track on a timeline (and its output pin on the node).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline
	 * @param OldTrackName - Current track name
	 * @param NewTrackName - New track name (must be unique on the timeline)
	 * @return True if the track was found and renamed
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool RenameTimelineTrack(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& OldTrackName,
		const FString& NewTrackName
	);

	/**
	 * Add a key to a vector track. (Same interp-mode options as add_timeline_float_key, applied to all 3 components.)
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline
	 * @param TrackName - Name of the vector track
	 * @param Time - Key time in seconds
	 * @param X / Y / Z - Component values
	 * @param InterpMode - "Auto" (smooth), "Linear", "Constant", "CubicUser"
	 * @return True if the key was added
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool AddTimelineVectorKey(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& TrackName,
		float Time,
		float X,
		float Y,
		float Z,
		const FString& InterpMode = TEXT("Auto")
	);

	/**
	 * Add a key to a linear-color track. (Interp mode applied to all 4 channels.)
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline
	 * @param TrackName - Name of the color track
	 * @param Time - Key time in seconds
	 * @param R / G / B / A - Channel values (0..1)
	 * @param InterpMode - "Auto" (smooth), "Linear", "Constant", "CubicUser"
	 * @return True if the key was added
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool AddTimelineColorKey(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& TrackName,
		float Time,
		float R,
		float G,
		float B,
		float A,
		const FString& InterpMode = TEXT("Auto")
	);

	/**
	 * Add a key (time only) to an event track.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline
	 * @param TrackName - Name of the event track
	 * @param Time - Time in seconds at which the event fires
	 * @return True if the key was added
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool AddTimelineEventKey(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& TrackName,
		float Time
	);

	/**
	 * Remove a key near a given time from a track of any type (all component curves for vector/color).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline
	 * @param TrackName - Name of the track
	 * @param Time - Key time in seconds
	 * @param Tolerance - Time tolerance for matching the key (default 0.001)
	 * @return True if at least one key was removed
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool RemoveTimelineKey(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& TrackName,
		float Time,
		float Tolerance = 0.001f
	);

	/**
	 * Remove all keys from a track (any type).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param TimelineName - Name of the timeline
	 * @param TrackName - Name of the track to clear
	 * @return True if the track was found and cleared
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool ClearTimelineTrackKeys(
		const FString& BlueprintPath,
		const FString& TimelineName,
		const FString& TrackName
	);

	// ============================================================================
	// GRAPH LAYOUT AUDIT
	// ============================================================================

	/**
	 * Measure the visual quality of a graph's current layout WITHOUT changing it.
	 * Returns a JSON report with: nodeCount, wireCount, execWireCount, nodeOverlaps
	 * (+ overlappingPairs), wireCrossings, backwardExecWires (+ list), longWires,
	 * total/avg wire length, execWireMeanAbsDeltaY (0 = perfectly straight exec
	 * backbone), graph bounds, an "issues" summary array (empty = layout looks
	 * clean), and per-node bounding boxes {id, title, x, y, width, height} so an
	 * agent can audit and correct placement.
	 *
	 * The engine's layout tools (BlueprintToolset.ArrangeGraphLayered / ArrangeNodes) only write a
	 * layout; nothing there reads one back, which is why this measurement is still here. Intended
	 * loop: author or arrange the graph (BlueprintDslToolset.Write, or
	 * BlueprintToolset.ArrangeGraphLayered) → analyze_graph_layout → if "issues" is non-empty, fix
	 * (BlueprintToolset.ArrangeNodes on the offenders, or SetNodePosition) and re-check. Comment
	 * boxes are ignored (decoration, not structure).
	 *
	 * Python Usage (bool return is folded away — the two out-strings come back as a tuple):
	 *   report, err = unreal.BlueprintService.analyze_graph_layout("/Game/BP_Player", "EventGraph")
	 *   data = json.loads(report)
	 *   assert data["nodeOverlaps"] == 0 and data["backwardExecWires"] == 0
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints|BatchGraph")
	static bool AnalyzeGraphLayout(
		const FString& BlueprintPath,
		const FString& GraphName,
		FString& OutReportJson,
		FString& OutError
	);




	/**
	 * Refresh the open Blueprint editor for a Blueprint so its SCS viewport re-runs the
	 * construction script and its graphs redraw. Use after authoring changes that the open editor
	 * does not reflect live. Returns False when the Blueprint has no editor open (there is nothing
	 * to refresh — the on-disk asset is already up to date).
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @return True if an editor was open and was refreshed
	 *
	 * Example:
	 *   unreal.BlueprintService.refresh_blueprint_editor("/Game/Props/BP_Refrigerator")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blueprints")
	static bool RefreshBlueprintEditor(const FString& BlueprintPath);


private:
	/** Helper to load blueprint from path */
	static UBlueprint* LoadBlueprint(const FString& BlueprintPath);

	/** Helper to find a graph by name */
	static UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& GraphName);

	/** Helper to find a node by ID in a graph */
	static UEdGraphNode* FindNodeById(UEdGraph* Graph, const FString& NodeId);

	/** Resolve a Custom Event node by blueprint/graph/GUID. Returns nullptr and sets OutError on failure. */
	static class UK2Node_CustomEvent* ResolveCustomEventNode(
		const FString& BlueprintPath,
		const FString& GraphName,
		const FString& NodeId,
		UBlueprint*& OutBlueprint,
		FString& OutError
	);

	// ── Batch Graph Builder internals ──

	/** Create a single node from a FGraphNodeDesc. Returns nullptr on failure. */
	static UEdGraphNode* CreateNodeFromDesc(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FGraphNodeDesc& Desc,
		float PosX, float PosY,
		FString& OutError
	);

	/** Resolve a pin by name with alias support (execute, then, value, true, false, etc.) */
	static UEdGraphPin* ResolvePinByName(
		UEdGraphNode* Node,
		const FString& PinName,
		EEdGraphPinDirection PreferredDirection = EGPD_MAX
	);

	/** Build a list of available pin names for error messages */
	static FString GetAvailablePinNames(UEdGraphNode* Node, EEdGraphPinDirection Direction);
};
