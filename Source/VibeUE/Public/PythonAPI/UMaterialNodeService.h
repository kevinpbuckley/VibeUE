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
