// Copyright Buckley Builds LLC 2025 All Rights Reserved.

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
 * Material Node Service - Python API for material graph manipulation
 * 
 * Provides 21 material node management actions:
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
 * - create_parameter: Create parameter expression
 * - promote_to_parameter: Promote constant to parameter
 * - set_parameter_metadata: Set parameter group/priority
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
	 * @param ParameterType Type (Scalar, Vector, Texture, StaticBool)
	 * @param ParameterName Name for the parameter
	 * @param GroupName Optional parameter group
	 * @param DefaultValue Default value as string
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
