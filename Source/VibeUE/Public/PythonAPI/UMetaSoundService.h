// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UMetaSoundService.generated.h"

// Forward declarations (full headers are pulled in by the .cpp only)
class UMetaSoundBuilderBase;
class UMetaSoundSource;

// ============================================================================
// RESULT / INFO STRUCTS
// ============================================================================

/** Result returned by mutating MetaSound operations */
USTRUCT(BlueprintType)
struct FMetaSoundResult
{
	GENERATED_BODY()

	/** Whether the operation succeeded */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	bool bSuccess = false;

	/** Human-readable result message */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString Message;

	/** Asset path of the affected asset (empty on failure) */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString AssetPath;

	/** GUID string of the node affected (set by add_node; empty for other operations) */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString NodeId;
};

/** Describes a single registered MetaSound node class available for use */
USTRUCT(BlueprintType)
struct FMetaSoundNodeClassInfo
{
	GENERATED_BODY()

	/** Node namespace, e.g. "Metasound.Standard" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString Namespace;

	/** Node name, e.g. "Sine" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString Name;

	/** Variant suffix, e.g. "Audio" — empty for most nodes */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString Variant;

	/** Full class name string as used by add_node, e.g. "Metasound.Standard.Sine:Audio" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString FullClassName;

	/** Major version */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	int32 MajorVersion = 1;

	/** Minor version */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	int32 MinorVersion = 0;

	/** Human-readable display name */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString DisplayName;

	/** Short description of what the node does */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString Description;

	/** Input pins: "PinName:DataType" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	TArray<FString> Inputs;

	/** Output pins: "PinName:DataType" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	TArray<FString> Outputs;
};

/** Summary information about a MetaSound source asset */
USTRUCT(BlueprintType)
struct FMetaSoundInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString AssetName;

	/** Audio output format: "Mono", "Stereo", or "Quad" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString OutputFormat;

	/** Total node count in the default graph page */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	int32 NodeCount = 0;

	/** Names of all graph-level inputs */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	TArray<FString> GraphInputs;

	/** Names of all graph-level outputs */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	TArray<FString> GraphOutputs;
};

/** Information about a single node inside a MetaSound graph */
USTRUCT(BlueprintType)
struct FMetaSoundNodeInfo
{
	GENERATED_BODY()

	/** GUID string — pass this to all node-targeting operations */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString NodeId;

	/** Editor display title */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString NodeTitle;

	/** Full class name, e.g. "Metasound.Standard.Sine:Audio" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString ClassName;

	/** Input pins: "PinName:DataType" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	TArray<FString> Inputs;

	/** Output pins: "PinName:DataType" */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	TArray<FString> Outputs;

	/** Editor graph X position */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	float PosX = 0.0f;

	/** Editor graph Y position */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	float PosY = 0.0f;
};

/** A single node input pin's current value: its literal default + whether it's wired. */
USTRUCT(BlueprintType)
struct FMetaSoundInputValue
{
	GENERATED_BODY()

	/** Input pin name. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString Name;

	/** Data type, e.g. "Float", "Audio", "Trigger". */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString DataType;

	/** Current literal default value as a string. Ignored at runtime when bIsConnected is true. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString DefaultValue;

	/** True if a connection feeds this input (the literal default is then overridden). */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	bool bIsConnected = false;
};

/** A single edge (wire) in the graph: FromNode.Output -> ToNode.Input. */
USTRUCT(BlueprintType)
struct FMetaSoundConnection
{
	GENERATED_BODY()

	/** GUID string of the source node. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString FromNodeId;

	/** Output pin name on the source node. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString FromOutput;

	/** GUID string of the destination node. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString ToNodeId;

	/** Input pin name on the destination node. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString ToInput;

	/** Data type carried by the connection. */
	UPROPERTY(BlueprintReadOnly, Category = "VibeUE|Audio|MetaSound")
	FString DataType;
};

// ============================================================================
// SERVICE
// ============================================================================

/**
 * Python-accessible MetaSound source authoring service.
 *
 * Quick reference (all methods are static — call on the class):
 *   r = unreal.MetaSoundService.create_meta_sound("/Game/Audio", "MS_Sine", "Mono")
 *   nodes = unreal.MetaSoundService.list_available_nodes("Sine")    # discover node class
 *   r2 = unreal.MetaSoundService.add_node(r.asset_path, "Metasound.Standard", "Sine", "Audio")
 *   # r2.node_id = GUID of the Sine node
 *   # connect Sine output to the built-in graph output node
 *   unreal.MetaSoundService.connect_nodes(r.asset_path, r2.node_id, "Out", graph_out_id, "Audio:0")
 *   unreal.MetaSoundService.save_meta_sound(r.asset_path)
 *
 * Use list_nodes() to retrieve NodeIds of the built-in interface nodes
 * (OnPlay output, OnFinished input, AudioOut inputs) created at asset creation.
 */
UCLASS()
class UMetaSoundService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =========================================================================
	// Lifecycle
	// =========================================================================

	/**
	 * Create a new MetaSound source asset on disk.
	 * The created source includes the standard interface nodes
	 * (On Play trigger output, On Finished trigger input, AudioOut input).
	 * @param PackagePath  Content path, e.g. "/Game/Audio/Ambience"
	 * @param AssetName    Asset name, e.g. "MS_Wind"
	 * @param OutputFormat "Mono" | "Stereo" | "Quad" (default "Mono")
	 * @return result.AssetPath = full content path of the new asset.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult CreateMetaSound(const FString& PackagePath,
	                                 const FString& AssetName,
	                                 const FString& OutputFormat = TEXT("Mono"));

	/** Delete a MetaSound source asset. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult DeleteMetaSound(const FString& AssetPath);

	/** Return summary info about a MetaSound source asset including node count and graph I/O names. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundInfo GetMetaSoundInfo(const FString& AssetPath);

	/**
	 * Save a MetaSound source asset after completing graph edits.
	 * Always call save after a series of add_node / connect_nodes operations.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult SaveMetaSound(const FString& AssetPath);

	// =========================================================================
	// Node Discovery
	// =========================================================================

	/**
	 * List all registered External (DSP) MetaSound node classes.
	 * Use this to discover Namespace / Name / Variant for add_node.
	 * @param SearchFilter Optional substring filter applied to FullClassName and DisplayName.
	 * @return Array of node class descriptors sorted alphabetically by FullClassName.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static TArray<FMetaSoundNodeClassInfo> ListAvailableNodes(const FString& SearchFilter = TEXT(""));

	// =========================================================================
	// Node Management
	// =========================================================================

	/**
	 * Add a DSP node to the MetaSound graph.
	 * @param NodeNamespace e.g. "Metasound.Standard"
	 * @param NodeName      e.g. "Sine"
	 * @param NodeVariant   e.g. "Audio" (use empty string when no variant)
	 * @param MajorVersion  Class major version (almost always 1)
	 * @param PosX / PosY   Editor position
	 * @return result.NodeId = GUID string of the new node.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult AddNode(const FString& AssetPath,
	                         const FString& NodeNamespace,
	                         const FString& NodeName,
	                         const FString& NodeVariant = TEXT(""),
	                         int32 MajorVersion = 1,
	                         float PosX = 0.0f,
	                         float PosY = 0.0f);

	/** Remove a node (and all its connected edges) from the graph. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult RemoveNode(const FString& AssetPath, const FString& NodeId);

	/** List all nodes currently in the default graph page. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static TArray<FMetaSoundNodeInfo> ListNodes(const FString& AssetPath);

	/** Return pin information for a single node. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundNodeInfo GetNodePins(const FString& AssetPath, const FString& NodeId);

	/**
	 * Read back the CURRENT literal default value of a single node input pin
	 * (the counterpart to set_node_input_default). Returns "" if the node/pin
	 * isn't found. If the input is connected, the value is the underlying default,
	 * not the connected signal — use get_node_input_values to see is_connected. (#460)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FString GetNodeInputDefault(const FString& AssetPath,
	                            const FString& NodeId,
	                            const FString& InputName);

	/**
	 * Read back every input pin on a node: name, data type, current literal default,
	 * and whether it is connected. (#460)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static TArray<FMetaSoundInputValue> GetNodeInputValues(const FString& AssetPath, const FString& NodeId);

	/**
	 * Read back every connection (edge) touching a node — both incoming (into its
	 * inputs) and outgoing (from its outputs) — with the resolved node ids and pin
	 * names on both ends. (#460)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static TArray<FMetaSoundConnection> GetNodeConnections(const FString& AssetPath, const FString& NodeId);

	// =========================================================================
	// Connections
	// =========================================================================

	/**
	 * Connect a node output pin to a node input pin.
	 * @param FromNodeId GUID string of the source node.
	 * @param OutputName Output pin name on the source node.
	 * @param ToNodeId   GUID string of the destination node.
	 * @param InputName  Input pin name on the destination node.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult ConnectNodes(const FString& AssetPath,
	                              const FString& FromNodeId,
	                              const FString& OutputName,
	                              const FString& ToNodeId,
	                              const FString& InputName);

	/** Disconnect the connection going INTO a specific node input pin. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult DisconnectPin(const FString& AssetPath,
	                               const FString& NodeId,
	                               const FString& InputName);

	// =========================================================================
	// Graph I/O
	// =========================================================================

	/**
	 * Add a named input to the graph (exposed as a parameter at runtime).
	 * @param DataType     e.g. "Float", "Int32", "Bool", "String", "Audio", "Trigger"
	 * @param DefaultValue String representation of default (e.g. "440.0" for Float)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult AddGraphInput(const FString& AssetPath,
	                               const FString& InputName,
	                               const FString& DataType,
	                               const FString& DefaultValue = TEXT(""));

	/**
	 * Add a named output to the graph.
	 * @param DataType e.g. "Float", "Audio", "Trigger"
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult AddGraphOutput(const FString& AssetPath,
	                                const FString& OutputName,
	                                const FString& DataType);

	/** Remove a named graph-level input. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult RemoveGraphInput(const FString& AssetPath, const FString& InputName);

	/** Remove a named graph-level output. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult RemoveGraphOutput(const FString& AssetPath, const FString& OutputName);

	// =========================================================================
	// Node Configuration
	// =========================================================================

	/**
	 * Set the default (literal) value on a node input pin.
	 * @param NodeId    GUID string of the node.
	 * @param InputName Pin name.
	 * @param Value     String value (e.g. "440.0", "true", "42", "Hello").
	 * @param DataType  "Float" | "Int32" | "Bool" | "String"
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult SetNodeInputDefault(const FString& AssetPath,
	                                     const FString& NodeId,
	                                     const FString& InputName,
	                                     const FString& Value,
	                                     const FString& DataType);

	/** Update the editor graph position of a node. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Audio|MetaSound")
	static FMetaSoundResult SetNodeLocation(const FString& AssetPath,
	                                 const FString& NodeId,
	                                 float PosX,
	                                 float PosY);

private:
	/** Load asset and begin a builder session. Returns nullptr and sets OutError on failure. */
	static UMetaSoundBuilderBase* BeginEditing(const FString& AssetPath, UMetaSoundSource** OutSource, FString& OutError);

	/** Register the graph and save the asset to disk. */
	static void CommitEditing(const FString& AssetPath, UMetaSoundSource* Source);

	/** Parse a GUID string, populating OutResult.Message on failure. */
	static bool ParseNodeGuid(const FString& NodeIdStr, FGuid& OutGuid, FMetaSoundResult& OutResult);

	/** Build a FMetaSoundNodeInfo from the builder, class metadata, and node data. */
	static FMetaSoundNodeInfo BuildNodeInfo(UMetaSoundBuilderBase* Builder,
	                                 const struct FMetasoundFrontendClass& Class,
	                                 const struct FMetasoundFrontendNode& Node);
};
