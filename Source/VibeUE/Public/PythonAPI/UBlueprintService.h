// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "Engine/Blueprint.h"
#include "UBlueprintService.generated.h"

/**
 * Information about a blueprint variable
 */
USTRUCT(BlueprintType)
struct FBlueprintVariableInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableType;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPublic = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsExposed = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;
};

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
 * Fixed-size overview of a single graph — the cheap first read before deciding
 * whether a full get_nodes_in_graph dump is needed. Payload size is independent
 * of graph size (the histogram/entry lists are one line per distinct type/event,
 * not per node).
 */
USTRUCT(BlueprintType)
struct FBlueprintGraphSummary
{
	GENERATED_BODY()

	/** Tab name as shown in the My Blueprint panel */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphName;

	/** "Ubergraph" | "Function" | "Macro" | "DelegateSignature" */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphKind;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 NodeCount = 0;

	/** Number of pin-to-pin links in the graph */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 ConnectionCount = 0;

	/** Blueprint-level compile status: UpToDate | UpToDateWithWarnings | Dirty | Error | Unknown */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString CompileStatus;

	/** Titles of entry-point nodes (events, custom events, function entry) with their node ids: "Title|NodeId" */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> EntryPoints;

	/** Node class histogram, most frequent first: "K2Node_CallFunction x12" */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> NodeTypeCounts;
};

/**
 * Detailed information about a blueprint variable (for get_info action)
 */
USTRUCT(BlueprintType)
struct FBlueprintVariableDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableType;

	/** Full type path (e.g., "/Script/CoreUObject.FloatProperty") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TypePath;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Tooltip;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;

	/** Whether the variable can be edited per instance in Details panel */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsInstanceEditable = false;

	/** Whether the variable is exposed on spawn */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsExposeOnSpawn = false;

	/** Whether the variable is private */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPrivate = false;

	/** Whether the variable is read-only in Blueprints */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsBlueprintReadOnly = false;

	/** Whether the variable is exposed to cinematics/Sequencer */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsExposeToCinematics = false;

	/** Replication condition: "None", "Replicated", or "RepNotify" */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ReplicationCondition;

	/** Whether this is an array type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsArray = false;

	/** Whether this is a set type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSet = false;

	/** Whether this is a map type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsMap = false;
};

/**
 * Search result for variable types
 */
USTRUCT(BlueprintType)
struct FVariableTypeInfo
{
	GENERATED_BODY()

	/** Type name (e.g., "Vector", "Actor") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TypeName;

	/** Full type path (e.g., "/Script/CoreUObject.Vector") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TypePath;

	/** Category (e.g., "Structure", "Object", "Enum") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Description of the type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Description;
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

/**
 * Information about a function that can be overridden in a blueprint.
 * Returned by list_overridable_functions — one entry per overridable parent function.
 */
USTRUCT(BlueprintType)
struct FOverridableFunctionInfo
{
	GENERATED_BODY()

	/** Function name (also the graph name when overridden) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FunctionName;

	/** C++ class that declares the function (e.g. "StateTreeTaskBlueprintBase") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString OwnerClass;

	/** Whether this function is already overridden in this blueprint */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bAlreadyOverridden = false;

	/** True for BlueprintNativeEvent (has C++ default), false for BlueprintImplementableEvent */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsNativeEvent = false;

	/**
	 * True if this override should be created as an event node in the EventGraph
	 * (void/latent functions with FUNC_Event flag — e.g. EnterState, StateCompleted, Tick).
	 * False if it should be a function graph with a return node (e.g. GetDescription).
	 * override_function() uses this automatically — no need to check manually.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsEventStyle = false;

	/** Return type as string (e.g. "FText", "bool", "void") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ReturnType;

	/** Parameter names and types as "Name:Type" strings */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Parameters;
};

/**
 * Information about a blueprint function
 */
USTRUCT(BlueprintType)
struct FVibeBlueprintFunctionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FunctionName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ReturnType;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Parameters;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsOverride = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPure = false;
};

/**
 * Information about a function local variable
 */
USTRUCT(BlueprintType)
struct FBlueprintLocalVariableInfo
{
	GENERATED_BODY()

	/** Variable name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableName;

	/** Friendly display name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FriendlyName;

	/** Type descriptor (e.g., "float", "struct:Vector", "object:Actor") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableType;

	/** Human-readable type string */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DisplayType;

	/** Default value as string */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;

	/** Category */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Variable GUID */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Guid;

	/** Whether variable is const/read-only */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsConst = false;

	/** Whether variable is a reference */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsReference = false;

	/** Whether variable is an array */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsArray = false;

	/** Whether variable is a set */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSet = false;

	/** Whether variable is a map */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsMap = false;
};

/**
 * Detailed information about a blueprint function (for get_info action)
 */
USTRUCT(BlueprintType)
struct FBlueprintFunctionDetailedInfo
{
	GENERATED_BODY()

	/** Function name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FunctionName;

	/** Graph GUID as string */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphGuid;

	/** Number of nodes in the function graph */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 NodeCount = 0;

	/** Whether this is a pure function */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPure = false;

	/** Whether this is an override */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsOverride = false;

	/** Input parameters */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintFunctionParameterInfo> InputParameters;

	/** Output parameters */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintFunctionParameterInfo> OutputParameters;

	/** Local variables */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintLocalVariableInfo> LocalVariables;
};

/**
 * Information about a blueprint component
 */
USTRUCT(BlueprintType)
struct FBlueprintComponentInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ComponentName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ComponentClass;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString AttachParent;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsRootComponent = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSceneComponent = false;

	/** True if this component comes from a parent C++ class and cannot be removed via Python APIs.
	 *  These are the "grayed out" components shown in the Blueprint Editor Components panel.
	 *  To replace an inherited root (e.g. DefaultSceneRoot), call set_root_component() instead. */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsInherited = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Children;
};

/**
 * Information about an available component type
 */
USTRUCT(BlueprintType)
struct FComponentTypeInfo
{
	GENERATED_BODY()

	/** Component class name (e.g., "StaticMeshComponent") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Name;

	/** Display name for UI */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DisplayName;

	/** Full class path (e.g., "/Script/Engine.StaticMeshComponent") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ClassPath;

	/** Category (e.g., "Rendering", "Physics") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Whether this is a scene component (can have transforms) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSceneComponent = false;

	/** Whether this is a primitive component (can render) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPrimitiveComponent = false;

	/** Whether this is an abstract class */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsAbstract = false;

	/** Base class name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString BaseClass;
};

/**
 * Detailed information about a component type
 */
USTRUCT(BlueprintType)
struct FComponentDetailedInfo
{
	GENERATED_BODY()

	/** Component class name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Name;

	/** Display name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DisplayName;

	/** Full class path */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ClassPath;

	/** Category */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Parent class name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ParentClass;

	/** Whether this is a scene component */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSceneComponent = false;

	/** Whether this is a primitive component */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPrimitiveComponent = false;

	/** Number of editable properties */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 PropertyCount = 0;

	/** Number of callable functions */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 FunctionCount = 0;
};

/**
 * Information about a component property
 */
USTRUCT(BlueprintType)
struct FComponentPropertyInfo
{
	GENERATED_BODY()

	/** Property name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PropertyName;

	/** Property type (e.g., "float", "FVector", "UStaticMesh*") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PropertyType;

	/** Property category */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Current value as string */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Value;

	/** Whether the property is editable */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsEditable = true;

	/** Whether the property is inherited */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsInherited = false;
};

/**
 * Information about a pin on a blueprint node
 */
USTRUCT(BlueprintType)
struct FBlueprintPinInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PinName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PinType;  // exec, bool, float, int, string, object, etc.

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsInput = true;  // True for input, false for output

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsConnected = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;

	/** Object/class default value as an object path; empty when the pin holds no object/class
	 *  default. Populated for PC_Object/PC_Class/PC_SoftObject/PC_SoftClass pins so get_node_pins
	 *  can report class/object references that the string-only DefaultValue does not capture. */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultObject;
};

/**
 * Information about a connection between two pins
 */
USTRUCT(BlueprintType)
struct FBlueprintConnectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString SourceNodeId;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString SourceNodeTitle;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString SourcePinName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TargetNodeId;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TargetNodeTitle;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TargetPinName;
};

/**
 * Information about a blueprint node
 */
USTRUCT(BlueprintType)
struct FBlueprintNodeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeId;  // Unique identifier (GUID)

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeType;  // K2Node class name

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeTitle;  // Display title

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	float PosX = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	float PosY = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> PinNames;  // Names of all pins on this node (for quick reference)

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintPinInfo> Pins;  // Detailed pin information
};

/**
 * Snapshot of the graph the user is currently looking at — VibeUE's equivalent of
 * Epic's FAIAssistantDockContext, expressed in the string-path conventions the rest
 * of this service uses so the result feeds straight into get_nodes_in_graph /
 * build_graph (for Blueprints) without translation. Covers the Blueprint family
 * (Blueprint / Widget / AnimBlueprint editors) and the Material editor.
 *
 * Returned by GetFocusedGraphContext(). bFound is false when no supported graph
 * editor is open and focused.
 */
USTRUCT(BlueprintType)
struct FBlueprintFocusContext
{
	GENERATED_BODY()

	/** True if an open Blueprint editor with a focused graph was found. */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bFound = false;

	/** Full object path of the focused Blueprint (feed directly to BlueprintPath params). */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString AssetPath;

	/** Short asset name (e.g. "BP_Player"). */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString AssetName;

	/** Editor flavour: "BlueprintEditor" | "WidgetBlueprintEditor" | "AnimationBlueprintEditor". */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString EditorType;

	/** Name of the focused graph tab (feed directly to GraphName params, e.g. "EventGraph"). */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphName;

	/** "Ubergraph" | "Function" | "Macro" | "DelegateSignature" | "Material" | "Other". */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphKind;

	/** Node count of the focused graph (cheap sanity signal). */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 GraphNodeCount = 0;

	/** The nodes currently selected in that graph (same shape as get_selected_nodes). */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintNodeInfo> SelectedNodes;
};

/**
 * Detailed pin information including connections
 */
USTRUCT(BlueprintType)
struct FBlueprintPinDetailedInfo
{
	GENERATED_BODY()

	/** Pin name (internal) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PinName;

	/** Pin display name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DisplayName;

	/** Pin type category (exec, bool, float, int, string, object, struct, etc.) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PinCategory;

	/** Pin type subcategory or object/struct path */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PinSubCategory;

	/** Full type path for struct/object types */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString TypePath;

	/** Whether this is an input pin */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsInput = true;

	/** Whether the pin is connected */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsConnected = false;

	/** Whether the pin is hidden */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsHidden = false;

	/** Whether this is an array type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsArray = false;

	/** Whether this is a reference type */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsReference = false;

	/** Whether the pin can be split (struct pins) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bCanSplit = false;

	/** Whether the pin is currently split */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsSplit = false;

	/** Default value */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DefaultValue;

	/** Tooltip */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Tooltip;

	/** Connected node IDs and pin names (format: "NodeId:PinName") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Connections;
};

/**
 * Detailed node information (for details action)
 */
USTRUCT(BlueprintType)
struct FBlueprintNodeDetailedInfo
{
	GENERATED_BODY()

	/** Node GUID */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeId;

	/** Node class name (e.g., K2Node_CallFunction) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeClass;

	/** Display title */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeTitle;

	/** Full title */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FullTitle;

	/** Graph name this node belongs to */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphName;

	/** Graph scope (event, function, macro) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString GraphScope;

	/** Node category */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Tooltip/description */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Tooltip;

	/** Position X */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	float PosX = 0.0f;

	/** Position Y */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	float PosY = 0.0f;

	/** Whether this is a pure node (no exec pins) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPure = false;

	/** Whether this node has latent execution */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsLatent = false;

	/** For function calls: target function name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FunctionName;

	/** For function calls: owning class */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString FunctionClass;

	/** For variable nodes: variable name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString VariableName;

	/** Input pins */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintPinDetailedInfo> InputPins;

	/** Output pins */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintPinDetailedInfo> OutputPins;
};

/**
 * Information about a discoverable node type
 */
USTRUCT(BlueprintType)
struct FBlueprintNodeTypeInfo
{
	GENERATED_BODY()

	/** Display name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString DisplayName;

	/** Node type category (e.g., "Math", "Flow Control") */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Category;

	/** Spawner key for creating this node */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString SpawnerKey;

	/** Node class name */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeClass;

	/** Tooltip/description */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Tooltip;

	/** Whether this is a pure function (no exec pins) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsPure = false;

	/** Whether this is a latent action */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsLatent = false;

	/** Keywords for searching */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Keywords;
};

/**
 * Result of compiling a blueprint - includes success status and any error/warning messages.
 */
USTRUCT(BlueprintType)
struct FBlueprintCompileResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 NumErrors = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 NumWarnings = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Errors;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Warnings;
};

/**
 * Comprehensive blueprint information
 */
USTRUCT(BlueprintType)
struct FBlueprintDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString BlueprintName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString BlueprintPath;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString ParentClass;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bIsWidgetBlueprint = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintVariableInfo> Variables;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FVibeBlueprintFunctionInfo> Functions;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FBlueprintComponentInfo> Components;
};

// ============================================================================
// BATCH GRAPH BUILDER TYPES
// ============================================================================

/**
 * Describes a single node to create in a batch graph operation.
 * Each node has a local ref (for wiring) and a type that maps to an
 * existing UBlueprintService creation method.
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
	 * "result_2", ...), each carrying params["existing"]="true". GetGraphDefinition emits
	 * them so a dumped function graph round-trips with its parameter/return wiring; BuildGraph
	 * binds them to the graph's own entry/result nodes instead of spawning new ones.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Type;

	/**
	 * Type-specific parameters (e.g. "class":"KismetMathLibrary", "function":"Clamp").
	 * Any node type also accepts an optional "group":"<title>" layout hint — after
	 * BuildGraph's auto-layout phase, each distinct title becomes a comment box
	 * wrapping its member nodes (GUID returned in RefToNodeId under "group:<title>").
	 * "existing":"true" marks a descriptor that BuildGraph resolves to a node already in
	 * the graph (used for function_entry/function_result) rather than creating a new one.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TMap<FString, FString> Params;
};

/**
 * Describes a connection between two node pins in a batch graph.
 * Format: "NodeRef.PinName" where NodeRef is either a local ref from the
 * Nodes array or an existing node's GUID string (32-char hex) already in the graph.
 */
USTRUCT(BlueprintType)
struct FGraphConnectionDesc
{
	GENERATED_BODY()

	/** Source (output) — format: "RefOrGUID.PinName" */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString From;

	/** Target (input) — format: "RefOrGUID.PinName" */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString To;
};

/**
 * Describes a pin default value to set in a batch graph.
 */
USTRUCT(BlueprintType)
struct FGraphPinDefaultDesc
{
	GENERATED_BODY()

	/** Local ref or existing GUID of the target node */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString NodeRef;

	/** Pin name on that node */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString PinName;

	/** Default value as string */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString Value;
};

/**
 * Result of a BuildGraph operation — full audit of what was created.
 */
USTRUCT(BlueprintType)
struct FBuildGraphResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 NodesCreated = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 NodesFailed = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 ConnectionsMade = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 ConnectionsFailed = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 DefaultsSet = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 DefaultsFailed = 0;

	/** Maps local ref → actual engine GUID string */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TMap<FString, FString> RefToNodeId;

	/** Error messages with actionable context */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Errors;

	/** Warning messages (non-fatal) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	TArray<FString> Warnings;

	/** Whether compilation was run */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bCompiled = false;

	/** Number of compilation errors (only if bCompiled) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 CompileErrors = 0;

	/** Number of compilation warnings (only if bCompiled) */
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	int32 CompileWarnings = 0;
};

/**
 * Blueprint service exposed directly to Python.
 *
 * This service provides blueprint introspection and analysis with native
 * Unreal Engine types.
 *
 * Python Usage:
 *   import unreal
 *
 *   # Get blueprint info (returns BlueprintDetailedInfo or None)
 *   info = unreal.BlueprintService.get_blueprint_info("/Game/Blueprints/BP_Player_Test")
 *   if info:
 *       print(f"Parent: {info.parent_class}")
 *       for var in info.variables:
 *           print(f"  {var.variable_name}: {var.variable_type}")
 *
 *   # List variables
 *   variables = unreal.BlueprintService.list_variables("/Game/BP_Player_Test")
 *
 *   # List components
 *   components = unreal.BlueprintService.list_components("/Game/BP_Player_Test")
 *
 * @note All methods are static and thread-safe
 * @note C++ out parameters become Python return values
 */
UCLASS(BlueprintType)
class VIBEUE_API UBlueprintService : public UToolsetDefinition
{
	GENERATED_BODY()

public:







	// ============================================================================
	// COMPONENT MANAGEMENT (manage_blueprint_component actions)
	// ============================================================================












	// ============================================================================
	// VARIABLE MANAGEMENT (Phase 1)
	// ============================================================================







	// ============================================================================
	// EVENT DISPATCHER MANAGEMENT
	// ============================================================================





	// ============================================================================
	// FUNCTION MANAGEMENT (Phase 2)
	// ============================================================================

	/**
	 * Add a macro graph to a Blueprint (typically a Macro Library Blueprint).
	 *
	 * Macro graphs are referenced by K2Node_MacroInstance nodes via add_macro_instance_node.
	 * Use this to create the macro graph, then reference it with the full path format:
	 *   "/Game/MyMacroLib.MyMacroLib:MyMacroName"
	 *
	 * To create a Macro Library Blueprint from Python:
	 *   factory = unreal.BlueprintMacroFactory()
	 *   factory.set_editor_property("parent_class", unreal.Actor.static_class())
	 *   tools = unreal.AssetToolsHelpers.get_asset_tools()
	 *   tools.create_asset("BPMacroLib", "/Game/Macros", unreal.Blueprint, factory)
	 *
	 * Note: do NOT use create_function() on a Macro Library — it asserts in the K2 schema.
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
	// NODE MANAGEMENT (Phase 3)
	// ============================================================================














	// ── Timelines ──

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
	// ADVANCED NODE OPERATIONS (Phase 4)
	// ============================================================================











	// ============================================================================
	// LIFECYCLE & PROPERTY MANAGEMENT (Missing manage_blueprint Actions)
	// ============================================================================




	// ============================================================================
	// NODE MANAGEMENT - Advanced Operations (manage_blueprint_node actions)
	// ============================================================================









	// ============================================================================
	// COMPONENT OPERATIONS - Extended API
	// ============================================================================




	// ============================================================================
	// EXISTENCE CHECKS - Fast boolean checks before creation (Idempotency)
	// ============================================================================








	// ============================================================================
	// FUNCTION OVERRIDES
	// ============================================================================



	// ================================================================
	// BATCH GRAPH BUILDER
	// ================================================================





	/**
	 * Measure the visual quality of a graph's current layout WITHOUT changing it.
	 * Returns a JSON report with: nodeCount, wireCount, execWireCount, nodeOverlaps
	 * (+ overlappingPairs), wireCrossings, backwardExecWires (+ list), longWires,
	 * total/avg wire length, execWireMeanAbsDeltaY (0 = perfectly straight exec
	 * backbone), graph bounds, an "issues" summary array (empty = layout looks
	 * clean), and per-node bounding boxes {id, title, x, y, width, height} so an
	 * agent can audit and correct placement.
	 *
	 * Intended loop: build_graph(auto_layout=True) → analyze_graph_layout → if
	 * "issues" is non-empty, fix (auto_layout_selected_nodes / set_node_position)
	 * and re-check. Comment boxes are ignored (decoration, not structure).
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
	 * Add a Blueprint Interface to a blueprint.
	 * Equivalent to: Class Settings → Interfaces → Add in the editor.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param InterfacePath - Interface asset path or short name (e.g., "BPI_TestInterface" or "/Game/interface/BPI_TestInterface")
	 * @return True if the interface was added successfully (or already implemented)
	 *
	 * Example:
	 *   unreal.BlueprintService.add_interface("/Game/BP_Player", "BPI_TestInterface")
	 *   unreal.BlueprintService.add_interface("/Game/BP_Player", "/Game/interface/BPI_TestInterface")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool AddInterface(
		const FString& BlueprintPath,
		const FString& InterfacePath
	);

	/**
	 * Remove a Blueprint Interface from a blueprint.
	 *
	 * @param BlueprintPath - Full path to the blueprint
	 * @param InterfacePath - Interface asset path or short name
	 * @return True if the interface was removed successfully
	 *
	 * Example:
	 *   unreal.BlueprintService.remove_interface("/Game/BP_Player", "BPI_TestInterface")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprints")
	static bool RemoveInterface(
		const FString& BlueprintPath,
		const FString& InterfacePath
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
