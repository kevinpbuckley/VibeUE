// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UMaterialNodeService.generated.h"

class UMaterial;
class UMaterialExpression;
struct FExpressionInput;

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

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Id;

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
 * - connect: Connect two expressions
 * - disconnect: Disconnect an input
 * - list_connections: List all connections
 * - connect_to_output: Connect expression to material output
 * - disconnect_output: Disconnect material output
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
 *   # Create expression
 *   expr = unreal.MaterialNodeService.create_expression("/Game/M_Test", "Constant3Vector", 0, 0)
 * 
 *   # Connect to material output
 *   unreal.MaterialNodeService.connect_to_output("/Game/M_Test", expr.id, "", "BaseColor")
 * 
 *   # Create function call node
 *   func = unreal.MaterialNodeService.create_function_call("/Game/M_Test", "/Engine/Functions/MF_Noise", -500, 0)
 * 
 *   # Batch create expressions
 *   nodes = unreal.MaterialNodeService.batch_create_expressions("/Game/M_Test", ["Add","Multiply","Constant"], [0,0,0], [-300,-200,-100], [0,200,400])
 * 
 * note: This replaces the JSON-based manage_material_node MCP tool
 */
UCLASS()
class VIBEUE_API UMaterialNodeService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Discovery Actions
	// =================================================================

	/**
	 * Discover available material expression types.
	 * Maps to action="discover_types"
	 * @param Category Optional category filter
	 * @param SearchTerm Optional search term (e.g., "Constant", "Parameter", "Texture")
	 * @param MaxResults Maximum results to return
	 * @return Array of expression type information
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static TArray<FMaterialExpressionTypeInfo> DiscoverTypes(
		const FString& Category = TEXT(""),
		const FString& SearchTerm = TEXT(""),
		int32 MaxResults = 100);

	/**
	 * Get all material expression categories.
	 * Maps to action="get_categories"
	 * @return Array of category names
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static TArray<FString> GetCategories();

	// =================================================================
	// Lifecycle Actions
	// =================================================================

	/**
	 * Create a new material expression.
	 * Maps to action="create"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionClass Class name (e.g., "Constant3Vector", "MaterialExpressionAdd")
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Created expression info (empty if failed)
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static FMaterialExpressionInfo CreateExpression(
		const FString& MaterialPath,
		const FString& ExpressionClass,
		int32 PosX = 0,
		int32 PosY = 0);

	/**
	 * Delete a material expression.
	 * Maps to action="delete"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression to delete
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static bool DeleteExpression(const FString& MaterialPath, const FString& ExpressionId);

	/**
	 * Move a material expression to a new position.
	 * Maps to action="move"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression to move
	 * @param PosX New X position
	 * @param PosY New Y position
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static bool MoveExpression(
		const FString& MaterialPath,
		const FString& ExpressionId,
		int32 PosX,
		int32 PosY);

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
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static FMaterialExpressionInfo CreateFunctionCall(
		const FString& MaterialPath,
		const FString& FunctionPath,
		int32 PosX = 0,
		int32 PosY = 0);

	/**
	 * Create a Custom HLSL expression node with code, output type, and optional inputs.
	 * Maps to action="create_custom_expression"
	 *
	 * @param MaterialPath Full path to the material
	 * @param Code HLSL code string (e.g., "return Input0 * 2.0;")
	 * @param OutputType Output type: "CMOT_Float1", "CMOT_Float2", "CMOT_Float3", "CMOT_Float4", "CMOT_MaterialAttributes"
	 * @param Description Display name for the node
	 * @param InputNames Comma-separated custom input names (e.g., "Albedo,Mask,UV") or empty for no custom inputs
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Created expression info
	 *
	 * Example:
	 *   node = unreal.MaterialNodeService.create_custom_expression("/Game/M_Test", "return Input0 * 2;", "CMOT_Float3", "DoubleIt", "Color", -500, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static FMaterialExpressionInfo CreateCustomExpression(
		const FString& MaterialPath,
		const FString& Code,
		const FString& OutputType = TEXT("CMOT_Float3"),
		const FString& Description = TEXT("Custom"),
		const FString& InputNames = TEXT(""),
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
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static FMaterialExpressionInfo CreateCollectionParameter(
		const FString& MaterialPath,
		const FString& CollectionPath,
		const FString& ParameterName,
		int32 PosX = 0,
		int32 PosY = 0);

	// =================================================================
	// Information Actions
	// =================================================================

	/**
	 * List all expressions in a material.
	 * Maps to action="list"
	 * @param MaterialPath Full path to the material
	 * @return Array of expression info
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static TArray<FMaterialExpressionInfo> ListExpressions(const FString& MaterialPath);

	/**
	 * Get detailed information about an expression.
	 * Maps to action="get_details"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression
	 * @param OutInfo Output expression info
	 * @return True if found
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static bool GetExpressionDetails(
		const FString& MaterialPath,
		const FString& ExpressionId,
		FMaterialExpressionInfo& OutInfo);

	/**
	 * Get all pins (inputs and outputs) for an expression.
	 * Maps to action="get_pins"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression
	 * @return Array of pin info
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static TArray<FMaterialNodePinInfo> GetExpressionPins(
		const FString& MaterialPath,
		const FString& ExpressionId);

	// =================================================================
	// Connection Actions
	// =================================================================

	/**
	 * Connect two material expressions.
	 * Maps to action="connect"
	 * @param MaterialPath Full path to the material
	 * @param SourceExpressionId Source expression ID
	 * @param SourceOutput Output name (empty for first output)
	 * @param TargetExpressionId Target expression ID
	 * @param TargetInput Input name on target
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static bool ConnectExpressions(
		const FString& MaterialPath,
		const FString& SourceExpressionId,
		const FString& SourceOutput,
		const FString& TargetExpressionId,
		const FString& TargetInput);

	/**
	 * Disconnect an input on an expression.
	 * Maps to action="disconnect"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId Expression with the input
	 * @param InputName Input to disconnect
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static bool DisconnectInput(
		const FString& MaterialPath,
		const FString& ExpressionId,
		const FString& InputName);

	/**
	 * List all connections in a material.
	 * Maps to action="list_connections"
	 * @param MaterialPath Full path to the material
	 * @return Array of connection info
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static TArray<FMaterialNodeConnectionInfo> ListConnections(const FString& MaterialPath);

	/**
	 * Connect an expression output to a material property (BaseColor, Roughness, etc.).
	 * Maps to action="connect_to_output"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId Source expression ID
	 * @param OutputName Output name (empty for first output)
	 * @param MaterialProperty Property name (BaseColor, Metallic, Roughness, etc.)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static bool ConnectToOutput(
		const FString& MaterialPath,
		const FString& ExpressionId,
		const FString& OutputName,
		const FString& MaterialProperty);

	/**
	 * Disconnect a material output property.
	 * Maps to action="disconnect_output"
	 * @param MaterialPath Full path to the material
	 * @param MaterialProperty Property name to disconnect
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static bool DisconnectOutput(
		const FString& MaterialPath,
		const FString& MaterialProperty);

	// =================================================================
	// Property Actions
	// =================================================================

	/**
	 * Get a property value from an expression.
	 * Maps to action="get_property"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression
	 * @param PropertyName Property name
	 * @return Property value as string
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static FString GetExpressionProperty(
		const FString& MaterialPath,
		const FString& ExpressionId,
		const FString& PropertyName);

	/**
	 * Set a property value on an expression.
	 * Maps to action="set_property"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression
	 * @param PropertyName Property name
	 * @param PropertyValue Value as string
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static bool SetExpressionProperty(
		const FString& MaterialPath,
		const FString& ExpressionId,
		const FString& PropertyName,
		const FString& PropertyValue);

	/**
	 * List all editable properties on an expression.
	 * Maps to action="list_properties"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression
	 * @return Array of property info
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static TArray<FMaterialNodePropertyInfo> ListExpressionProperties(
		const FString& MaterialPath,
		const FString& ExpressionId);

	// =================================================================
	// Parameter Actions
	// =================================================================

	/**
	 * Create a parameter expression directly.
	 * Maps to action="create_parameter"
	 * @param MaterialPath Full path to the material
	 * @param ParameterType Type: "Scalar", "Vector", "Texture", "TextureObject", "StaticBool", "StaticSwitch"
	 * @param ParameterName Name for the parameter
	 * @param GroupName Optional parameter group
	 * @param DefaultValue Default value as string. For Scalar: "0.5". For Vector/Color:
	 *        "(R=1.0,G=0.0,B=0.0,A=1.0)" or "1.0,0.0,0.0,1.0" or "1.0,0.0,0.0" (alpha defaults to 1.0)
	 *        or "1.0/0.0/0.0". For StaticBool/StaticSwitch: "true"/"false".
	 *        For Texture/TextureObject: asset path "/Game/Textures/T_Tex"
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Created parameter info
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static FMaterialExpressionInfo CreateParameter(
		const FString& MaterialPath,
		const FString& ParameterType,
		const FString& ParameterName,
		const FString& GroupName = TEXT(""),
		const FString& DefaultValue = TEXT(""),
		int32 PosX = 0,
		int32 PosY = 0);

	/**
	 * Promote a constant expression to a parameter.
	 * Maps to action="promote_to_parameter"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId Expression to promote (must be Constant, Constant3Vector, etc.)
	 * @param ParameterName Name for the new parameter
	 * @param GroupName Optional parameter group
	 * @return New parameter expression info
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static FMaterialExpressionInfo PromoteToParameter(
		const FString& MaterialPath,
		const FString& ExpressionId,
		const FString& ParameterName,
		const FString& GroupName = TEXT(""));

	/**
	 * Set parameter metadata (group, sort priority).
	 * Maps to action="set_parameter_metadata"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId Parameter expression ID
	 * @param GroupName New group name
	 * @param SortPriority Sort priority within group
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static bool SetParameterMetadata(
		const FString& MaterialPath,
		const FString& ExpressionId,
		const FString& GroupName,
		int32 SortPriority = 0);

	// =================================================================
	// Material Output Actions
	// =================================================================

	/**
	 * Get available material output properties.
	 * Maps to action="get_output_properties"
	 * @param MaterialPath Full path to the material
	 * @return Array of property names (BaseColor, Metallic, Roughness, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static TArray<FString> GetOutputProperties(const FString& MaterialPath);

	/**
	 * Get current connections to material outputs.
	 * Maps to action="get_output_connections"
	 * @param MaterialPath Full path to the material
	 * @return Array of output connection info
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static TArray<FMaterialOutputConnectionInfo> GetOutputConnections(const FString& MaterialPath);

	// =================================================================
	// Batch Operations
	// =================================================================

	/**
	 * Batch-create multiple expressions in one call for performance.
	 * Maps to action="batch_create"
	 *
	 * All nodes are created in a single transaction with one graph refresh,
	 * yielding 10-100x speedup over individual create calls for large materials.
	 *
	 * @param MaterialPath Full path to the material
	 * @param ExpressionClasses Array of class names (e.g., ["Add", "Multiply", "Constant"])
	 * @param PosXArray Array of X positions (same length as ExpressionClasses)
	 * @param PosYArray Array of Y positions (same length as ExpressionClasses)
	 * @return Array of created expression info in same order
	 *
	 * Example:
	 *   nodes = unreal.MaterialNodeService.batch_create_expressions("/Game/M_Test",
	 *       ["Add","Multiply","Constant"], [-300,-200,-100], [0,200,400])
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static TArray<FMaterialExpressionInfo> BatchCreateExpressions(
		const FString& MaterialPath,
		const TArray<FString>& ExpressionClasses,
		const TArray<int32>& PosXArray,
		const TArray<int32>& PosYArray);

	/**
	 * Batch-connect multiple expression pairs in one call.
	 * Maps to action="batch_connect"
	 *
	 * All connections are made in a single transaction with one graph refresh.
	 *
	 * @param MaterialPath Full path to the material
	 * @param SourceIds Array of source expression IDs
	 * @param SourceOutputs Array of source output names (empty string for first output)
	 * @param TargetIds Array of target expression IDs
	 * @param TargetInputs Array of target input names
	 * @return Number of successful connections
	 *
	 * Example:
	 *   count = unreal.MaterialNodeService.batch_connect_expressions("/Game/M_Test",
	 *       [n1.id, n2.id], ["", ""], [n3.id, n3.id], ["A", "B"])
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static int32 BatchConnectExpressions(
		const FString& MaterialPath,
		const TArray<FString>& SourceIds,
		const TArray<FString>& SourceOutputs,
		const TArray<FString>& TargetIds,
		const TArray<FString>& TargetInputs);

	/**
	 * Batch-set properties on multiple expressions in one call.
	 * Maps to action="batch_set_properties"
	 *
	 * All property changes are made in a single transaction with one graph refresh.
	 * The arrays must all be the same length (each index is one set operation).
	 *
	 * @param MaterialPath Full path to the material
	 * @param ExpressionIds Array of expression IDs
	 * @param PropertyNames Array of property names
	 * @param PropertyValues Array of property values as strings
	 * @return Number of successful property sets
	 *
	 * Example:
	 *   count = unreal.MaterialNodeService.batch_set_properties("/Game/M_Test",
	 *       [n1.id, n1.id, n2.id], ["R", "G", "Bias"], ["True", "False", "0.5"])
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static int32 BatchSetProperties(
		const FString& MaterialPath,
		const TArray<FString>& ExpressionIds,
		const TArray<FString>& PropertyNames,
		const TArray<FString>& PropertyValues);

	/**
	 * Batch-create expressions of all types (including specialized) in one transaction.
	 * Maps to action="batch_create_specialized"
	 *
	 * Supports generic expressions, MaterialFunctionCall (with function path),
	 * Custom HLSL (with code, output type, inputs), and CollectionParameter
	 * (with collection path and parameter name). All created in a single transaction
	 * with one graph refresh for 10-100x speedup.
	 *
	 * For generic expressions, only ClassName/PosX/PosY are needed.
	 * For MaterialFunctionCall, also set FunctionPath.
	 * For Custom, also set HLSLCode, HLSLOutputType, HLSLDescription, HLSLInputNames.
	 * For CollectionParameter, also set CollectionPath, CollectionParamName.
	 *
	 * @param MaterialPath Full path to the material
	 * @param Descriptors Array of creation descriptors
	 * @return Array of created expression info in same order (empty entries on failure)
	 *
	 * Example:
	 *   descs = [FBatchCreateDescriptor(ClassName="Multiply", PosX=-400, PosY=0),
	 *            FBatchCreateDescriptor(ClassName="MaterialFunctionCall", PosX=-600, PosY=0, FunctionPath="/Game/MF_Test")]
	 *   nodes = unreal.MaterialNodeService.batch_create_specialized("/Game/M_Test", descs)
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static TArray<FMaterialExpressionInfo> BatchCreateSpecialized(
		const FString& MaterialPath,
		const TArray<FBatchCreateDescriptor>& Descriptors);

	// =================================================================
	// Export Actions
	// =================================================================

	/**
	 * Export the complete material graph as a JSON string for analysis or recreation.
	 * Maps to action="export_graph"
	 *
	 * Returns material settings, all expressions (type, position, properties),
	 * all connections, and material output connections — everything needed to
	 * recreate the material from scratch.
	 *
	 * @param MaterialPath Full path to the material
	 * @return JSON string of complete graph, or empty string on failure
	 *
	 * Example:
	 *   json_str = unreal.MaterialNodeService.export_material_graph("/Game/M_Landscape")
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static FString ExportMaterialGraph(const FString& MaterialPath);

	/**
	 * Export a lightweight summary of a material graph (counts, types, parameters).
	 * Maps to action="export_material_graph_summary"
	 *
	 * Returns a JSON object with:
	 * - expression_count: total expressions
	 * - class_counts: {class_name: count} frequency map
	 * - connection_count: total connections
	 * - material_output_count: connected material outputs
	 * - parameter_count: number of scalar/vector/texture/static parameters
	 * - parameters: [{name, type}] list
	 *
	 * Much lighter than ExportMaterialGraph — useful for verification and comparison.
	 *
	 * @param MaterialPath Full path to the material
	 * @return JSON string summary, or empty string on failure
	 *
	 * Example:
	 *   summary = unreal.MaterialNodeService.export_material_graph_summary("/Game/M_Mat")
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static FString ExportMaterialGraphSummary(const FString& MaterialPath);

	/**
	 * Compare two material graphs and return a JSON diff report.
	 * Maps to action="compare_material_graphs"
	 *
	 * Compares expression counts by class, connection counts, parameter lists,
	 * and material output assignments. Returns JSON with:
	 * - match: true/false overall
	 * - expression_count_match, connection_count_match, parameter_match, output_match
	 * - expression_count_a, expression_count_b
	 * - class_diff: [{class_name, count_a, count_b}] for mismatches
	 * - missing_parameters_in_b, extra_parameters_in_b: [{name, type}]
	 *
	 * @param MaterialPathA Full path to first material (reference)
	 * @param MaterialPathB Full path to second material (recreation)
	 * @return JSON diff string, or empty string on failure
	 *
	 * Example:
	 *   diff = unreal.MaterialNodeService.compare_material_graphs("/Game/M_Original", "/Game/M_Copy")
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static FString CompareMaterialGraphs(
		const FString& MaterialPathA,
		const FString& MaterialPathB);

	// =================================================================
	// Layout Actions
	// =================================================================

	/**
	 * Auto-arrange all material expressions into a clean left-to-right layout.
	 * Walks the connection graph from material outputs back through inputs,
	 * positions nodes in columns by depth, and stacks vertically within columns.
	 * Maps to action="layout_expressions"
	 * @param MaterialPath Full path to the material
	 * @param ColumnSpacing Horizontal spacing between columns (default 300)
	 * @param RowSpacing Vertical spacing between nodes in same column (default 180)
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.MaterialNodeService.layout_expressions("/Game/Materials/M_Terrain")
	 *   unreal.MaterialNodeService.layout_expressions("/Game/Materials/M_Terrain", 400, 200)
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialNode")
	static bool LayoutExpressions(
		const FString& MaterialPath,
		int32 ColumnSpacing = 300,
		int32 RowSpacing = 180);

private:
	// Helper methods
	static UMaterial* LoadMaterialAsset(const FString& MaterialPath);
	static UMaterialExpression* FindExpressionById(UMaterial* Material, const FString& ExpressionId);
	static FString GetExpressionId(UMaterialExpression* Expression);
	static FExpressionInput* FindInputByName(UMaterialExpression* Expression, const FString& InputName);
	static int32 FindOutputIndexByName(UMaterialExpression* Expression, const FString& OutputName);
	static TArray<FString> GetExpressionInputNames(UMaterialExpression* Expression);
	static TArray<FString> GetExpressionOutputNames(UMaterialExpression* Expression);
	static UClass* ResolveExpressionClass(const FString& ClassName);
	static FMaterialExpressionInfo BuildExpressionInfo(UMaterialExpression* Expression);
	static EMaterialProperty StringToMaterialProperty(const FString& PropertyName);
	static void RefreshMaterialGraph(UMaterial* Material);
};
