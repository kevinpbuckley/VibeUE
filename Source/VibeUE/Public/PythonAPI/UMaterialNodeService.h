// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UMaterialNodeService.generated.h"

class UMaterial;
class UMaterialExpression;
class UMaterialFunction;
struct FExpressionInput;

/**
 * Information about a single input or output on a Material Function
 */
USTRUCT(BlueprintType)
struct FMaterialFunctionPinInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Description;

	/** Numeric input type (EFunctionInputType: 0=Scalar, 1=Vector2, 2=Vector3, 3=Vector4, 4=Texture2D, etc.) */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 InputType = 0;

	/** Human-readable type name (e.g., "Scalar", "Vector3", "Texture2D") */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString InputTypeName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 SortPriority = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Id;
};

/**
 * Information about a Material Function's interface
 */
USTRUCT(BlueprintType)
struct FVibeUEMaterialFunctionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString FunctionPath;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FMaterialFunctionPinInfo> Inputs;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FMaterialFunctionPinInfo> Outputs;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 ExpressionCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bExposeToLibrary = false;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> LibraryCategories;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ErrorMessage;
};

/**
 * Material expression type information for discovery
 */
USTRUCT(BlueprintType)
struct FMaterialExpressionTypeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ClassName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bIsParameter = false;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> Keywords;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> InputNames;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> OutputNames;
};

/**
 * Information about a material expression (node) in the material graph
 */
USTRUCT(BlueprintType)
struct FMaterialExpressionInfo
{
	GENERATED_BODY()

	/** Session-specific id ("<Class>_<pointer>"). Stable within one editor session but NOT across
	 *  restarts — persist ObjectPath if you need a durable reference. Accepted by the id-based lookups. */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Id;

	/** Full object path of this expression (e.g. "/Game/M.M:MaterialExpressionConstant3Vector_0").
	 *  Session-independent; also accepted anywhere an ExpressionId is taken. */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ObjectPath;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ClassName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 PosX = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 PosY = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bIsParameter = false;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ParameterName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> InputNames;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> OutputNames;
};

/**
 * Information about an input or output pin on a material expression
 */
USTRUCT(BlueprintType)
struct FMaterialNodePinInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 Index = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Direction;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ValueType;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bIsConnected = false;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ConnectedExpressionId;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 ConnectedOutputIndex = -1;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString DefaultValue;
};

/**
 * Information about a connection between material expressions
 */
USTRUCT(BlueprintType)
struct FMaterialNodeConnectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString SourceExpressionId;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString SourceOutput;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString TargetExpressionId;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString TargetInput;
};

/**
 * Expression property info
 */
USTRUCT(BlueprintType)
struct FMaterialNodePropertyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Value;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString PropertyType;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bIsEditable = true;
};

/**
 * Material output connection info
 */
USTRUCT(BlueprintType)
struct FMaterialOutputConnectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString PropertyName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ConnectedExpressionId;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bIsConnected = false;
};

/**
 * Descriptor for batch-creating specialized expressions (function calls, custom HLSL, collection params).
 * Used with BatchCreateSpecialized to create all node types in a single transaction.
 */
USTRUCT(BlueprintType)
struct FBatchCreateDescriptor
{
	GENERATED_BODY()

	/** Expression class name (e.g., "Multiply", "MaterialFunctionCall", "Custom", "CollectionParameter") */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ClassName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 PosX = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 PosY = 0;

	/** For MaterialFunctionCall: path to the material function asset */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString FunctionPath;

	/** For Custom: HLSL code string */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString HLSLCode;

	/** For Custom: output type ("CMOT_Float1", "CMOT_Float2", "CMOT_Float3", "CMOT_Float4", "CMOT_MaterialAttributes") */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString HLSLOutputType;

	/** For Custom: display description */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString HLSLDescription;

	/** For Custom: comma-separated input names */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString HLSLInputNames;

	/** For Custom: semicolon-separated additional outputs, each as "Name:Type" (e.g., "WorldHeight:Float1;Mask:Float3") */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString HLSLAdditionalOutputs;

	/** For CollectionParameter: path to the MaterialParameterCollection asset */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString CollectionPath;

	/** For CollectionParameter: parameter name within the collection */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString CollectionParamName;
};

/**
 * Material Node Service - Python API for material graph manipulation
 * 
 * Provides 37 material node management actions:
 * 
 * Discovery:
 * - discover_types: Find available material expression types
 * - get_categories: Get expression categories
 * 
 * Lifecycle:
 * - create: Create a new material expression
 * - delete: Delete an expression
 * - move: Move expression to new position
 * 
 * Specialized Creation:
 * - create_function_call: Create a MaterialFunctionCall node referencing a function asset
 * - create_custom_expression: Create a Custom HLSL expression node
 * - create_collection_parameter: Create a CollectionParameter referencing a parameter collection
 * 
 * Information:
 * - list: List all expressions in material
 * - get_details: Get detailed expression info
 * - get_pins: Get expression pin information
 * 
 * Connections:
 * - list_connections: List all connections
 * - batch_connect_expressions: Connect multiple expression pairs (expression-to-expression)
 * - connect_expression_to_output: Connect an expression output to a material output (BaseColor, Normal, ...)
 * - disconnect_output: Clear a material output's input
 * 
 * Properties:
 * - get_property: Get expression property value
 * - set_property: Set expression property value
 * - list_properties: List all expression properties
 * 
 * Parameters:
 * - create_parameter: Create parameter expression (Scalar, Vector, Texture, TextureObject, StaticBool, StaticSwitch)
 * - promote_to_parameter: Promote constant to parameter
 * - set_parameter_metadata: Set parameter group/priority
 * 
 * Batch Operations:
 * - batch_create: Create multiple expressions in one call
 * - batch_connect: Connect multiple expression pairs in one call
 * - batch_set_properties: Set properties on multiple expressions in one call
 * - batch_create_specialized: Create all expression types (including function calls, custom HLSL, collection params) in one call
 * 
 * Export:
 * - export_graph: Export complete material graph as JSON for recreation
 * - export_graph_summary: Export lightweight summary (counts, types) without full property data
 * - compare_graphs: Compare two material graphs and report differences
 * 
 * Layout:
 * - layout_expressions: Auto-arrange all nodes in a clean left-to-right flow
 * 
 * Material Outputs:
 * - get_output_properties: Get available material output pins
 * - get_output_connections: Get current output connections
 * 
 * Python Usage:
 *   import unreal
 * 
 *   # Discover expression types
 *   types = unreal.MaterialNodeService.discover_types("", "Constant", 20)
 * 
 *   # Create expressions (batch)
 *   nodes = unreal.MaterialNodeService.batch_create_expressions("/Game/M_Test", ["Constant3Vector"], [0], [0])
 *   expr = nodes[0]
 *
 *   # Connect an expression output to a material output.
 *   # expr.id is session-specific; expr.object_path is durable and also accepted here.
 *   unreal.MaterialNodeService.connect_expression_to_output("/Game/M_Test", expr.id, "", "BaseColor")
 *   # ... or clear it again:
 *   unreal.MaterialNodeService.disconnect_output("/Game/M_Test", "BaseColor")
 *
 *   # Create function call node
 *   func = unreal.MaterialNodeService.create_function_call("/Game/M_Test", "/Engine/Functions/MF_Noise", -500, 0)
 * 
 *   # Batch create expressions
 *   nodes = unreal.MaterialNodeService.batch_create_expressions("/Game/M_Test", ["Add","Multiply","Constant"], [0,0,0], [-300,-200,-100], [0,200,400])
 * 
 * note: This replaces the JSON-based manage_material_node MCP tool
 */

/**
 * Comprehensive material compile/sampler diagnostics. Use this to verify a material is
 * actually compiling and sampling textures — replaces the unreliable
 * `MaterialEditingLibrary.get_used_textures` signal which returns 0 for many graphs
 * that nonetheless render correctly.
 */
USTRUCT(BlueprintType)
struct FMaterialDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	bool bIsCompiledOk = false;

	/** Compile errors from the most recent compilation attempt. Empty when bIsCompiledOk. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	TArray<FString> CompileErrors;

	/** All texture asset paths the material actually references (after compile). Reliable. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	TArray<FString> ReferencedTexturePaths;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 ExpressionCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 TextureSampleCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 TextureObjectParameterCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 FunctionCallCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 ScalarParameterCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 VectorParameterCount = 0;
};

UCLASS()
class VIBEUE_API UMaterialNodeService : public UToolsetDefinition
{
	GENERATED_BODY()

public:


	// =================================================================
	// Discovery Actions
	// =================================================================



	// =================================================================
	// Specialized Creation Actions
	// =================================================================

	/**
	 * Create a MaterialFunctionCall expression referencing a material function asset.
	 * Maps to action="create_function_call"
	 *
	 * The function reference is loaded and set automatically, which creates the
	 * correct input/output pins matching the function's interface.
	 *
	 * @param MaterialPath Full path to the material
	 * @param FunctionPath Full path to the material function asset (e.g., "/Engine/Functions/Engine_MaterialFunctions03/Procedurals/NoiseFunctions")
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Created expression info with function's inputs/outputs
	 *
	 * Example:
	 *   func = unreal.MaterialNodeService.create_function_call("/Game/M_Test", "/Game/MF_TerrainLayer", -500, 0)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FMaterialExpressionInfo CreateFunctionCall(
		const FString& MaterialPath,
		const FString& FunctionPath,
		int32 PosX = 0,
		int32 PosY = 0);


	/**
	 * Create a CollectionParameter expression referencing a MaterialParameterCollection.
	 * Maps to action="create_collection_parameter"
	 *
	 * @param MaterialPath Full path to the material
	 * @param CollectionPath Full path to the MaterialParameterCollection asset
	 * @param ParameterName Name of the parameter within the collection to use
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Created expression info
	 *
	 * Example:
	 *   node = unreal.MaterialNodeService.create_collection_parameter("/Game/M_Test", "/Game/MPC_Weather", "WindStrength", -500, 0)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FMaterialExpressionInfo CreateCollectionParameter(
		const FString& MaterialPath,
		const FString& CollectionPath,
		const FString& ParameterName,
		int32 PosX = 0,
		int32 PosY = 0);

	// =================================================================
	// Information Actions
	// =================================================================




	// =================================================================
	// Connection Actions
	// =================================================================


	// =================================================================
	// Property Actions
	// =================================================================




	// =================================================================
	// Parameter Actions
	// =================================================================




	// =================================================================
	// Material Output Actions
	// =================================================================





	// =================================================================
	// Batch Operations
	// =================================================================





	// =================================================================
	// Export Actions
	// =================================================================




	// =================================================================
	// Material Function Actions
	// =================================================================










	/**
	 * The single source of truth for the material outputs this service understands, as ordered
	 * (friendly name -> EMaterialProperty) pairs. Both the writers (StringToMaterialProperty, behind
	 * connect_expression_to_output / disconnect_output) and the reader (GetOutputConnections) are
	 * driven from it, so a property can never be writable but invisible again (issue #611).
	 * Not a UFUNCTION (TPair does not marshal); public so a regression test can assert that parity.
	 */
	static const TArray<TPair<FString, EMaterialProperty>>& GetMaterialOutputProperties();

	/** Strict material-property mapper, driven by GetMaterialOutputProperties(). Accepts friendly
	 *  ("BaseColor") and enum ("MP_BaseColor") spellings, case-insensitive. Returns false and leaves
	 *  OutProperty untouched for unknown names — a bad name must never silently rewire another output.
	 *  Not a UFUNCTION (EMaterialProperty out-param); public so the parity test can assert against it. */
	static bool StringToMaterialProperty(const FString& PropertyName, EMaterialProperty& OutProperty);

private:
	// Helper methods
	static UMaterial* LoadMaterialAsset(const FString& MaterialPath);
	static UMaterialFunction* LoadMaterialFunctionAsset(const FString& FunctionPath);
	static UMaterialExpression* FindExpressionById(UMaterial* Material, const FString& ExpressionId);
	static UMaterialExpression* FindExpressionInFunctionById(UMaterialFunction* Function, const FString& ExpressionId);
	static FString GetExpressionId(UMaterialExpression* Expression);
	static FExpressionInput* FindInputByName(UMaterialExpression* Expression, const FString& InputName);
	static int32 FindOutputIndexByName(UMaterialExpression* Expression, const FString& OutputName);
	static TArray<FString> GetExpressionInputNames(UMaterialExpression* Expression);
	static TArray<FString> GetExpressionOutputNames(UMaterialExpression* Expression);
	static UClass* ResolveExpressionClass(const FString& ClassName);
	static FMaterialExpressionInfo BuildExpressionInfo(UMaterialExpression* Expression);
	static void RefreshMaterialGraph(UMaterial* Material);
	static FString FunctionInputTypeToString(int32 InputType);
	static int32 StringToFunctionInputType(const FString& TypeName);
};
