// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UCustomizableObjectService.generated.h"

/** One end of a pin link: the node on the other side and its pin. */
USTRUCT(BlueprintType)
struct FVibeCOPinLink
{
	GENERATED_BODY()

	/** NodeId (the node's NodeGuid) of the linked node. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString NodeId;

	/** PinName of the linked pin on that node. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString PinName;
};

/** One pin on a Customizable Object graph node. */
USTRUCT(BlueprintType)
struct FVibeCOPinInfo
{
	GENERATED_BODY()

	/**
	 * The pin's internal name — what ConnectPins / DisconnectPins match first. Often differs from the
	 * label the editor shows: a Skeletal Mesh Section node's mesh input is "Mesh_Input_Pin".
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString PinName;

	/** The label the graph editor draws. Also accepted wherever a pin name is. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString DisplayName;

	/** "input" or "output". */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Direction;

	/**
	 * Mutable pin category, e.g. "object", "component", "material" (a mesh section), "mesh",
	 * "skeletalMesh", "passThroughSkeletalMesh", "image", "color", "float", "enum", "modifier",
	 * "materialAsset", "string", "transform", "projector", "wildcard". Two pins connect when the
	 * categories match (plus a few pass-through special cases the schema allows).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Category;

	/** Pin sub-category, e.g. "overrideMaterial" / "overlayMaterial". Usually empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString SubCategory;

	/** True for an input that accepts many links (e.g. an Object node's "Components"). */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bIsArray = false;

	/** Hidden pins exist and can be linked, but the editor does not draw them. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bHidden = false;

	/** Every pin this one is linked to. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FVibeCOPinLink> LinkedTo;
};

/** One node in a Customizable Object's editor graph. */
USTRUCT(BlueprintType)
struct FVibeCONodeInfo
{
	GENERATED_BODY()

	/** The node's NodeGuid (32 hex digits). The id every other method takes. Stable across saves. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString NodeId;

	/** The node's UObject name, e.g. "CONodeSwitch_0". Also accepted as a node id. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString NodeName;

	/** Node class as reflection names it, without the U prefix, e.g. "CONodeSkeletalMeshSection". */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString NodeClass;

	/** The title the graph editor draws. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	int32 PosX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	int32 PosY = 0;

	/** False for the Base Object node, which the editor refuses to delete. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bCanDelete = true;

	/**
	 * The node's graph-editor error badge, if any. Usually empty: Mutable reports compile problems in
	 * CompileObject's Errors / Warnings (which end with "(Node <title>)"), not on the nodes.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FVibeCOPinInfo> Pins;
};

/** One directed link, output pin to input pin. */
USTRUCT(BlueprintType)
struct FVibeCOConnectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString FromNodeId;

	/** Output pin name on FromNodeId. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString FromPin;

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString ToNodeId;

	/** Input pin name on ToNodeId. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString ToPin;
};

/** A whole Customizable Object graph: nodes plus a flat connection list. */
USTRUCT(BlueprintType)
struct FVibeCOGraphInfo
{
	GENERATED_BODY()

	/** False if the asset could not be loaded or has no graph; see Error. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Error;

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FVibeCONodeInfo> Nodes;

	/** Each link once, output side first. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FVibeCOConnectionInfo> Connections;
};

/** One node class that AddNode can create. */
USTRUCT(BlueprintType)
struct FVibeCONodeTypeInfo
{
	GENERATED_BODY()

	/** The name AddNode takes, e.g. "CONodeSwitch" or "CustomizableObjectNodeFloatParameter". */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString ClassName;

	/** The class's editor display name. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString DisplayName;

	/** The class tooltip, when the engine declares one. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Tooltip;

	/**
	 * False for classes the editor palette never offers (NotPlaceable / HideDropdown — the typed
	 * switch subclasses, for example, which are made as a CONodeSwitch with a PinType instead).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bPlaceable = true;

	/** True for a legacy class the engine migrates on load. Prefer its replacement. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bDeprecated = false;

	/** The module the class lives in, e.g. "CustomizableObjectEditor". */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Module;
};

/** One property of a node (or node class default), with its current value. */
USTRUCT(BlueprintType)
struct FVibeCOPropertyInfo
{
	GENERATED_BODY()

	/** Property name, as SetNodeProperty / AddNode's PropertiesJson take it. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Name;

	/** C++ type, e.g. "FString", "TSoftObjectPtr<USkeletalMesh>", "TArray<FCustomizableObjectState>". */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Type;

	/** Current value as text, in the same format SetNodeProperty accepts. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Value;

	/**
	 * True if the details panel shows it directly. Several authoring properties (ObjectName,
	 * ParameterName, GroupName, ComponentName, PinType) are edited through custom UI instead, so
	 * this is false for them and they are still settable.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bEditable = false;

	/** The class that declares the property. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString DeclaredBy;
};

/** Outcome of any graph write. */
USTRUCT(BlueprintType)
struct FVibeCOEditResult
{
	GENERATED_BODY()

	/** True only if the edit was applied AND the asset saved. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bSuccess = false;

	/** Populated whenever bSuccess is false. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Error;

	/** The node the edit created or addressed. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString NodeId;

	/** SetNodeProperty: the value read back after the node processed the change. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString ValueAfterWrite;

	/** Side effects worth knowing about, e.g. links the schema broke to make a connection. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FString> Notes;

	/** AddNode / RefreshNode / SetNodeProperty: the node's pins after the edit. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FVibeCOPinInfo> Pins;
};

/** A parameter, either authored in the graph or exposed by the compiled object. */
USTRUCT(BlueprintType)
struct FVibeCOParameterInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Name;

	/**
	 * Graph parameters: the node class ("EnumParameter", "FloatParameter", ...).
	 * Compiled parameters: the EMutableParameterType name ("Int" is an enum parameter).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Type;

	/** Enum parameters: the option names. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FString> Options;

	/** Graph parameters: the NodeId of the parameter node. Empty for compiled parameters. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString NodeId;
};

/** Summary of a Customizable Object asset. */
USTRUCT(BlueprintType)
struct FVibeCOObjectInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Error;

	/** True when the object holds a valid compiled model (the compiled lists below are filled). */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bIsCompiled = false;

	/** True when the Base Object node names a ParentObject: this asset extends another. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bIsChildObject = false;

	/** The parent Customizable Object's path, for a child object. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString ParentObject;

	/** Saved Customizable Objects whose Base Object node names this one as ParentObject. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FString> ChildObjects;

	/** NodeId of the Base Object node. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString BaseNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	int32 NodeCount = 0;

	/** Parameter nodes in this asset's graph (authoring view; no compile needed). */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FVibeCOParameterInfo> GraphParameters;

	/** Parameters of the compiled model, including those contributed by child objects. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FVibeCOParameterInfo> CompiledParameters;

	/** States of the compiled model. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FString> States;

	/** Mesh components of the compiled model. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FString> Components;
};

/** Outcome of CompileObject. */
USTRUCT(BlueprintType)
struct FVibeCOCompileResult
{
	GENERATED_BODY()

	/** True only when the object compiled to a valid model AND no error was reported. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bSuccess = false;

	/** "Completed" or "Failed". */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString State;

	/** Why the compile could not even be requested (asset missing, Mutable unavailable). */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	FString Error;

	/** IsCompiled() after the call. A failed compile clears the model, so this goes false. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	bool bIsCompiled = false;

	/** Error-severity compiler messages. Most name the offending node. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FString> Errors;

	/** Warning-severity compiler messages. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FString> Warnings;

	/** Every Mutable log line emitted during the compile, in order, errors and warnings included. */
	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	TArray<FString> Log;

	UPROPERTY(BlueprintReadOnly, Category = "Mutable")
	float DurationSeconds = 0.0f;
};

/**
 * Read, author and compile Mutable Customizable Object graphs.
 *
 * Mutable is an optional engine plugin (Beta). In UE 5.8 it is normally loaded anyway, because the
 * enabled-by-default MutableAssetUserData plugin depends on it, but a project can turn it off. This
 * service reaches it through reflection only — VibeUE does not link against or enable Mutable — so
 * it builds and loads either way, and every method reports "Mutable is not available" when it is off.
 * Node classes are addressed by their reflected name (DiscoverNodeTypes lists them), nodes by their
 * NodeGuid (NodeId), pins by PinName or display name. Every write saves the asset.
 *
 * Methods:
 * - is_mutable_available: whether the Mutable plugin is loaded
 * - list_customizable_objects: Customizable Object assets under a directory
 * - create_customizable_object: new asset with its graph and Base Object node
 * - get_object_info: parent/children, parameters, states, components, compile status
 * - get_graph / get_node: nodes, pins and connections
 * - discover_node_types / get_node_type_properties: node class catalog and class defaults
 * - add_node / remove_node / set_node_position / refresh_node: node edits
 * - connect_pins / disconnect_pins: wiring
 * - get_node_properties / set_node_property: node property read/write
 * - set_parent_object: make an asset a child object of another object's group
 * - compile_object: synchronous compile returning errors, warnings and the compile log
 *
 * Example:
 *   unreal.CustomizableObjectService.get_graph("/Game/Characters/CO_Hero")
 */
UCLASS(BlueprintType)
class VIBEUE_API UCustomizableObjectService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// ---- Availability & assets ----------------------------------------------------------------

	/** True when the Mutable plugin (CustomizableObject + CustomizableObjectEditor modules) is loaded. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static bool IsMutableAvailable();

	/** Package paths of every Customizable Object asset under DirectoryPath (recursive), sorted. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static TArray<FString> ListCustomizableObjects(const FString& DirectoryPath = TEXT("/Game"));

	/**
	 * Create a Customizable Object at AssetPath (e.g. "/Game/Characters/CO_Hero") through the engine's
	 * own factory, so it gets its graph and the undeletable Base Object node. Saved immediately.
	 * Refuses to overwrite an existing asset. NodeId in the result is the Base Object node.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOEditResult CreateCustomizableObject(const FString& AssetPath);

	/**
	 * Parent/child relationships, graph parameters, and — when the object is compiled — its compiled
	 * parameters (with enum options), states and components. Read-only.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOObjectInfo GetObjectInfo(const FString& AssetPath);

	// ---- Graph reads --------------------------------------------------------------------------

	/** Every node (with pins and links) plus a flat connection list. Read-only. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOGraphInfo GetGraph(const FString& AssetPath);

	/** One node by NodeId (NodeGuid) or node name. Check NodeId on the result: empty means not found. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCONodeInfo GetNode(const FString& AssetPath, const FString& NodeId);

	/**
	 * Node classes AddNode accepts, optionally filtered by a case-insensitive substring of the class
	 * or display name. Non-placeable and legacy classes are left out unless bIncludeHidden.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static TArray<FVibeCONodeTypeInfo> DiscoverNodeTypes(const FString& Filter = TEXT(""),
		bool bIncludeHidden = false);

	/** Properties of a node class with their default values — what AddNode's PropertiesJson can set. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static TArray<FVibeCOPropertyInfo> GetNodeTypeProperties(const FString& NodeClass);

	/** Properties of one node in the graph, with current values. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static TArray<FVibeCOPropertyInfo> GetNodeProperties(const FString& AssetPath, const FString& NodeId);

	// ---- Graph writes (each one saves the asset) ----------------------------------------------

	/**
	 * Add a node of NodeClass at (PosX, PosY). PropertiesJson is an optional JSON object of
	 * property -> value applied BEFORE the node builds its pins — required for the nodes whose pins
	 * depend on it: CONodeSwitch needs {"PinType": "<pin category>"} (e.g. "material", "color",
	 * "mesh", "image", "float", "component"), and a Skeletal Mesh node given {"SkeletalMesh": path}
	 * is created with one output pin per LOD/section. Values are strings in property text format;
	 * JSON numbers and booleans are converted. The result lists the new node's pins.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOEditResult AddNode(const FString& AssetPath, const FString& NodeClass,
		int32 PosX = 0, int32 PosY = 0, const FString& PropertiesJson = TEXT(""));

	/** Break every link on the node and delete it. The Base Object node cannot be deleted. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOEditResult RemoveNode(const FString& AssetPath, const FString& NodeId);

	/** Move a node in the graph editor. Cosmetic; does not affect the compiled object. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOEditResult SetNodePosition(const FString& AssetPath, const FString& NodeId,
		int32 PosX, int32 PosY);

	/**
	 * Link an output pin to an input pin (either order is accepted). Pins are matched by PinName,
	 * then case-insensitively, then by display name. The schema's own rules apply: a single-link
	 * input drops its previous link, and any links broken to make room are listed in Notes.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOEditResult ConnectPins(const FString& AssetPath, const FString& FromNodeId,
		const FString& FromPin, const FString& ToNodeId, const FString& ToPin);

	/**
	 * Break links on a pin. With OtherNodeId/OtherPin, only that one link; otherwise every link on
	 * the pin.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOEditResult DisconnectPins(const FString& AssetPath, const FString& NodeId,
		const FString& PinName, const FString& OtherNodeId = TEXT(""), const FString& OtherPin = TEXT(""));

	/**
	 * Set one node property from text (the format GetNodeProperties reports), then let the node react
	 * exactly as it would to a details-panel edit — a Skeletal Mesh node rebuilds its LOD/section pins,
	 * a Section node its material-parameter pins, and Switch nodes fed by an edited Enum Parameter
	 * rebuild their option pins. A value that fails to parse is rolled back. ValueAfterWrite is read
	 * back; Pins are the node's pins afterwards.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOEditResult SetNodeProperty(const FString& AssetPath, const FString& NodeId,
		const FString& PropertyName, const FString& Value);

	/** Rebuild a node's pins (ReconstructNode), keeping links whose pins still exist. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOEditResult RefreshNode(const FString& AssetPath, const FString& NodeId);

	/**
	 * Make ChildAssetPath a child object of ParentAssetPath: its Base Object node's ParentObject is
	 * set, and ParentObjectGroupId to the Group node GroupNodeId in the parent's graph. Pass an empty
	 * ParentAssetPath to detach. Compile the ROOT object to see the child's contribution.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOEditResult SetParentObject(const FString& ChildAssetPath, const FString& ParentAssetPath,
		const FString& GroupNodeId);

	// ---- Compile ------------------------------------------------------------------------------

	/**
	 * Compile synchronously and return the compiler's errors, warnings and log. Always a real
	 * compile: the engine's "skip if not out of date" shortcut is disabled, because it only notices
	 * SAVED changes and would otherwise report success for a stale model.
	 * OptimizationLevel: "None" (fast, default), "Maximum" or "FromCustomizableObject".
	 * TextureCompression: "Fast" (default), "None" or "HighQuality".
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Mutable")
	static FVibeCOCompileResult CompileObject(const FString& AssetPath,
		const FString& OptimizationLevel = TEXT("None"), const FString& TextureCompression = TEXT("Fast"));
};
