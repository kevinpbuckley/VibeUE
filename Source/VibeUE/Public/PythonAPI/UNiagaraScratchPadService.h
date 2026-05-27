// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UNiagaraScratchPadService.generated.h"

/**
 * Result of a scratch-pad operation that creates or mutates a node / module.
 * Mirrors the lightweight result-struct convention used by MaterialNodeService.
 */
USTRUCT(BlueprintType)
struct FNiagaraScratchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	bool bSuccess = false;

	/** Human/AI readable status or error message. */
	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString Message;

	/** For node operations: the persistent GUID of the affected node (use as NodeId in later calls). */
	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString NodeId;

	/** For module operations: the stack module (function-call) name. */
	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString ModuleName;

	/** For module operations: the object path of the underlying scratch UNiagaraScript. */
	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString ScriptPath;
};

/**
 * A node inside a scratch-pad script graph.
 */
USTRUCT(BlueprintType)
struct FNiagaraScratchNodeInfo
{
	GENERATED_BODY()

	/** Persistent node GUID - stable address used by all node/pin operations. */
	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString NodeId;

	/** Short class label: "MapGet", "MapSet", "Op", "CustomHlsl", "If", "Input", "Output", "FunctionCall", or the raw class name. */
	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString NodeType;

	/** Display title of the node. */
	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString Title;

	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	int32 PosX = 0;

	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	int32 PosY = 0;

	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	bool bIsEnabled = true;
};

/**
 * An input or output pin on a scratch-pad node.
 */
USTRUCT(BlueprintType)
struct FNiagaraScratchPinInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString PinName;

	/** "Input" or "Output". */
	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString Direction;

	/** Niagara type name, e.g. "float", "Vector", "Position", "Parameter Map". */
	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString TypeName;

	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	bool bIsConnected = false;

	/** Default literal value for unconnected input pins (where available). */
	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString DefaultValue;

	/** Persistent pin GUID. */
	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString PinId;
};

/**
 * A wire between two pins in a scratch-pad graph.
 */
USTRUCT(BlueprintType)
struct FNiagaraScratchConnectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString FromNodeId;

	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString FromPin;

	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString ToNodeId;

	UPROPERTY(BlueprintReadWrite, Category = "NiagaraScratch")
	FString ToPin;
};

/**
 * Niagara Scratch Pad Service - Python API for authoring scratch-pad module graphs.
 *
 * The existing NiagaraEmitterService can add/list/configure stack *modules*, but it cannot
 * reach inside a scratch-pad module to build node-based logic or set Custom HLSL. This service
 * fills that gap: it resolves a stack module to its underlying scratch UNiagaraScript, then
 * lets you create nodes, set Custom HLSL, add typed pins, and wire pins together - the same
 * operations MaterialNodeService provides for material graphs.
 *
 * Typical workflow (build a Custom-HLSL splat module):
 *   import unreal
 *   S = "/Game/VFX/NS_TrackPainter"
 *   E = "CompletelyEmpty"
 *   # 1. Create an empty scratch module on the Particle Update stage
 *   r = unreal.NiagaraScratchPadService.create_scratch_module(S, E, "ParticleUpdate", "SplatLine")
 *   mod = r.module_name
 *   # 2. Drop a Custom HLSL node and set the splat code
 *   hlsl = unreal.NiagaraScratchPadService.add_custom_hlsl_node(S, E, mod, CODE, 300, 0)
 *   # 3. Declare typed inputs/outputs on the Custom HLSL node
 *   unreal.NiagaraScratchPadService.add_pin(S, E, mod, hlsl.node_id, "Input", "Vector", "StartPos")
 *   unreal.NiagaraScratchPadService.add_pin(S, E, mod, hlsl.node_id, "Output", "float", "Result")
 *   # 4. Expose stack inputs and wire them in
 *   unreal.NiagaraScratchPadService.add_module_input(S, E, mod, "TireRadius", "float")
 *   unreal.NiagaraScratchPadService.connect_pins(S, E, mod, mapget_id, "Module.TireRadius", hlsl.node_id, "TireRadius")
 *   # 5. Apply + compile
 *   unreal.NiagaraScratchPadService.apply_changes(S)
 *
 * All graph mutations operate on the scratch script asset directly (no open editor required).
 * Call apply_changes() once at the end to refresh signatures, rebuild emitter nodes, recompile,
 * and save. Discover method signatures with discover_python_class('unreal.NiagaraScratchPadService').
 */
UCLASS(BlueprintType)
class VIBEUE_API UNiagaraScratchPadService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Scratch module lifecycle
	// =================================================================

	/**
	 * Create a new (empty) scratch-pad module and add it to an emitter's stack.
	 *
	 * Creates a scratch UNiagaraScript (from the engine's default module template), stores it in the
	 * emitter's scratch-pad container, and inserts it as a stack module at the end of the chosen stage.
	 *
	 * @param SystemPath  Full path to the Niagara system
	 * @param EmitterName Name of the emitter
	 * @param Stage       "ParticleSpawn", "ParticleUpdate", "EmitterSpawn", or "EmitterUpdate"
	 * @param DesiredName Suggested module name (a unique name is generated if empty/taken)
	 * @return Result with module_name and script_path on success
	 *
	 * Example:
	 *   r = unreal.NiagaraScratchPadService.create_scratch_module("/Game/VFX/NS_Fx", "Main", "ParticleUpdate", "SplatLine")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Create Scratch Module"))
	static FNiagaraScratchResult CreateScratchModule(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& Stage,
		const FString& DesiredName = TEXT("ScratchModule"));

	/**
	 * Resolve the object path of the scratch UNiagaraScript backing a stack module.
	 * (NiagaraEmitterService.get_module_info leaves ScriptAssetPath empty for scratch modules.)
	 *
	 * @return Script object path, or empty string if the module is not a scratch module.
	 *
	 * Example:
	 *   path = unreal.NiagaraScratchPadService.get_scratch_script_path("/Game/VFX/NS_Fx", "Main", "SplatLine")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Get Scratch Script Path"))
	static FString GetScratchScriptPath(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName);

	/**
	 * List the names of all scratch-pad modules present on an emitter's stacks.
	 *
	 * Example:
	 *   mods = unreal.NiagaraScratchPadService.list_scratch_modules("/Game/VFX/NS_Fx", "Main")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "List Scratch Modules"))
	static TArray<FString> ListScratchModules(
		const FString& SystemPath,
		const FString& EmitterName);

	// =================================================================
	// Graph inspection
	// =================================================================

	/**
	 * List every node in a scratch module's graph.
	 *
	 * Example:
	 *   nodes = unreal.NiagaraScratchPadService.list_nodes("/Game/VFX/NS_Fx", "Main", "SplatLine")
	 *   for n in nodes: print(n.node_type, n.node_id, n.title)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "List Scratch Nodes"))
	static TArray<FNiagaraScratchNodeInfo> ListNodes(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName);

	/**
	 * Get the input/output pins of a single node.
	 *
	 * @param NodeId Persistent node GUID from list_nodes / a create call
	 *
	 * Example:
	 *   pins = unreal.NiagaraScratchPadService.get_node_pins("/Game/VFX/NS_Fx", "Main", "SplatLine", node_id)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Get Scratch Node Pins"))
	static TArray<FNiagaraScratchPinInfo> GetNodePins(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& NodeId);

	/**
	 * List all wires (pin-to-pin connections) in a scratch module's graph.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "List Scratch Connections"))
	static TArray<FNiagaraScratchConnectionInfo> ListConnections(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName);

	// =================================================================
	// Node authoring
	// =================================================================

	/**
	 * Create a node of a simple type in the scratch graph.
	 *
	 * @param NodeType One of: "MapGet" (Parameter Map Get), "MapSet" (Parameter Map Set),
	 *                 "If" (branch), "Input" (constant/parameter input).
	 *                 Use add_op_node for math and add_custom_hlsl_node for HLSL.
	 * @return Result with node_id on success
	 *
	 * Example:
	 *   r = unreal.NiagaraScratchPadService.add_node("/Game/VFX/NS_Fx", "Main", "SplatLine", "MapGet", 0, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Add Scratch Node"))
	static FNiagaraScratchResult AddNode(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& NodeType,
		int32 PosX = 0,
		int32 PosY = 0);

	/**
	 * Create a math operation node (UNiagaraNodeOp).
	 *
	 * @param OpName Registered op name, e.g. "Numeric::Add", "Numeric::Multiply", "Numeric::Subtract",
	 *               "Numeric::Divide", "Numeric::Dot", "Numeric::Cross", "Numeric::Normalize",
	 *               "Numeric::Length", "Numeric::Lerp", "Numeric::Clamp", "Numeric::Min", "Numeric::Max".
	 *               Pass just "Add" etc. and the service will try the "Numeric::" namespace automatically.
	 *
	 * Example:
	 *   r = unreal.NiagaraScratchPadService.add_op_node("/Game/VFX/NS_Fx", "Main", "SplatLine", "Numeric::Multiply", 200, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Add Scratch Op Node"))
	static FNiagaraScratchResult AddOpNode(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& OpName,
		int32 PosX = 0,
		int32 PosY = 0);

	/**
	 * Create a Custom HLSL node and set its code. Add typed input/output pins afterwards with add_pin.
	 *
	 * IMPORTANT: Niagara Custom HLSL is NOT plain HLSL. Reference pins by their pin name as variables,
	 * assign results to output-pin-named variables, and use Niagara helpers (e.g. data-interface
	 * function calls) rather than raw resource intrinsics. See the niagara-emitters skill for the dialect.
	 *
	 * @param Code Initial HLSL body
	 * @return Result with node_id on success
	 *
	 * Example:
	 *   r = unreal.NiagaraScratchPadService.add_custom_hlsl_node(S, E, mod, "Result = A * B;", 300, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Add Custom HLSL Node"))
	static FNiagaraScratchResult AddCustomHlslNode(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& Code,
		int32 PosX = 0,
		int32 PosY = 0);

	/**
	 * Replace the HLSL body of an existing Custom HLSL node.
	 *
	 * @param NodeId Node GUID of a Custom HLSL node
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Set Custom HLSL Code"))
	static bool SetCustomHlslCode(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& NodeId,
		const FString& Code);

	/**
	 * Read the HLSL body of a Custom HLSL node.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Get Custom HLSL Code"))
	static FString GetCustomHlslCode(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& NodeId);

	/**
	 * Add a typed input or output pin to a node that supports dynamic pins (Custom HLSL, Map Get, Map Set).
	 *
	 * For Custom HLSL nodes the pin name becomes the variable name in the HLSL body.
	 * For Map Get / Map Set the pin name should be a namespaced parameter (e.g. "Module.TireRadius",
	 * "Output.SplatLine.Result"); if you pass a bare name the service applies the node's default namespace.
	 *
	 * @param Direction "Input" or "Output"
	 * @param TypeName  "float","int","bool","Vector"/"vec3","vec2","vec4","Position","Color","LinearColor",
	 *                  or a data-interface type: "ArrayFloat","ArrayVector","ArrayInt","Grid2D","RenderTarget2D"
	 * @return Result with pin info in Message on success
	 *
	 * Example:
	 *   unreal.NiagaraScratchPadService.add_pin(S, E, mod, hlsl_id, "Input", "Vector", "StartPos")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Add Scratch Pin"))
	static FNiagaraScratchResult AddPin(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& NodeId,
		const FString& Direction,
		const FString& TypeName,
		const FString& PinName);

	/**
	 * Delete a node from the scratch graph.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Delete Scratch Node"))
	static bool DeleteNode(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& NodeId);

	/**
	 * Move a node to a new graph position (purely cosmetic).
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Set Scratch Node Position"))
	static bool SetNodePosition(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& NodeId,
		int32 PosX,
		int32 PosY);

	// =================================================================
	// Wiring
	// =================================================================

	/**
	 * Connect an output pin of one node to an input pin of another (validated through the Niagara schema).
	 *
	 * @param FromNodeId/FromPin  Source node GUID and its OUTPUT pin name
	 * @param ToNodeId/ToPin      Target node GUID and its INPUT pin name
	 * @return True if the schema accepted and created the connection
	 *
	 * Example:
	 *   unreal.NiagaraScratchPadService.connect_pins(S, E, mod, mapget_id, "Module.TireRadius", hlsl_id, "TireRadius")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Connect Scratch Pins"))
	static bool ConnectPins(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& FromNodeId,
		const FString& FromPin,
		const FString& ToNodeId,
		const FString& ToPin);

	/**
	 * Break all links on a single pin.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Disconnect Scratch Pin"))
	static bool DisconnectPin(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& NodeId,
		const FString& PinName);

	// =================================================================
	// Module signature helpers (high level)
	// =================================================================

	/**
	 * Expose a stack input on the module by adding a "Module.<InputName>" read to the module's Map Get node
	 * (creating the Map Get node if necessary). After apply_changes the input appears in the stack UI and can
	 * be driven from NiagaraEmitterService.set_module_input or linked to a User parameter.
	 *
	 * @return Result whose node_id is the Map Get node; the new output pin is named "Module.<InputName>"
	 *
	 * Example:
	 *   r = unreal.NiagaraScratchPadService.add_module_input(S, E, mod, "TireRadius", "float")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Add Module Input"))
	static FNiagaraScratchResult AddModuleInput(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& InputName,
		const FString& TypeName);

	/**
	 * Write a value into the parameter map from the module by adding an input pin to the module's Map Set node
	 * (creating it if necessary) and threading the parameter map through it.
	 *
	 * @param OutputName Namespaced write target, e.g. "Particles.MyAttribute" or "Output.<Module>.X".
	 *                   A bare name is placed in the Map Set node's default namespace.
	 * @return Result whose node_id is the Map Set node; the new input pin is named after OutputName
	 *
	 * Example:
	 *   r = unreal.NiagaraScratchPadService.add_module_output(S, E, mod, "Particles.Splatted", "bool")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Add Module Output"))
	static FNiagaraScratchResult AddModuleOutput(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& OutputName,
		const FString& TypeName);

	// =================================================================
	// Apply / compile
	// =================================================================

	/**
	 * Finalize scratch-pad edits: refresh every stack module that references a scratch script, rebuild the
	 * system's emitter compiled nodes, recompile, and save. Call this once after a batch of graph edits.
	 *
	 * @return True if recompilation succeeded with no errors
	 *
	 * Example:
	 *   ok = unreal.NiagaraScratchPadService.apply_changes("/Game/VFX/NS_Fx")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraScratch", meta = (DisplayName = "Apply Scratch Changes"))
	static bool ApplyChanges(const FString& SystemPath);

	// (All internal helpers are file-static in UNiagaraScratchPadService.cpp.)
};
