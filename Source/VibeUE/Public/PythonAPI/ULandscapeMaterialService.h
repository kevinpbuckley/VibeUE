// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ULandscapeMaterialService.generated.h"

class UMaterial;
class UMaterialExpression;
class UMaterialExpressionLandscapeLayerBlend;

/**
 * Result of landscape material creation
 */
USTRUCT(BlueprintType)
struct FLandscapeMaterialCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	FString ErrorMessage;
};

/**
 * Configuration for a landscape material layer
 */
USTRUCT(BlueprintType)
struct FLandscapeMaterialLayerConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	FString LayerName;

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	FString BlendType;

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	float PreviewWeight = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	bool bUseHeightBlend = false;
};

/**
 * Information about a LandscapeLayerBlend node
 */
USTRUCT(BlueprintType)
struct FLandscapeLayerBlendInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	FString NodeId;

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	TArray<FLandscapeMaterialLayerConfig> Layers;
};

/**
 * Result of layer info object creation
 */
USTRUCT(BlueprintType)
struct FLandscapeLayerInfoCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	FString LayerName;

	UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
	FString ErrorMessage;
};

/**
 * Landscape material service exposed directly to Python.
 *
 * Provides 17 landscape material management actions:
 *
 * Material Creation:
 * - create_landscape_material: Create a material configured for landscape use
 *
 * Layer Blend Node:
 * - create_layer_blend_node: Create a LandscapeLayerBlend expression
 * - create_layer_blend_node_with_layers: Create blend node with all layers in one call
 * - add_layer_to_blend_node: Add a layer to an existing blend node
 * - remove_layer_from_blend_node: Remove a layer from a blend node
 * - get_layer_blend_info: Get info about a blend node's layers
 * - connect_to_layer_input: Connect an expression to a layer's input on blend node
 *
 * Coordinates:
 * - create_layer_coords_node: Create landscape UV coordinate expression
 *
 * Layer Sample:
 * - create_layer_sample_node: Create a LandscapeLayerSample expression for sampling layer weight
 *
 * Grass Output:
 * - create_grass_output: Create a LandscapeGrassOutput expression with grass type mappings
 *
 * Layer Info Objects:
 * - create_layer_info_object: Create a ULandscapeLayerInfoObject asset
 * - get_layer_info_details: Get details about a layer info object
 *
 * Assignment:
 * - assign_material_to_landscape: Assign material to landscape with layer mapping
 *
 * Convenience:
 * - setup_layer_textures: Set up complete layer with diffuse/normal/roughness
 *
 * Weight Node:
 * - create_layer_weight_node: Create a LandscapeLayerWeight expression
 *
 * Existence:
 * - landscape_material_exists: Check if a material exists
 * - layer_info_exists: Check if a layer info object exists
 *
 * Python Usage:
 *   import unreal
 *
 *   # Create landscape material
 *   mat = unreal.LandscapeMaterialService.create_landscape_material("M_Terrain", "/Game/Materials")
 *
 *   # Add layer blend
 *   blend = unreal.LandscapeMaterialService.create_layer_blend_node(mat.asset_path)
 *   unreal.LandscapeMaterialService.add_layer_to_blend_node(mat.asset_path, blend.node_id, "Grass")
 *
 *   # Create layer info objects
 *   info = unreal.LandscapeMaterialService.create_layer_info_object("Grass", "/Game/Landscape")
 */
UCLASS(BlueprintType)
class VIBEUE_API ULandscapeMaterialService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Material Creation
	// =================================================================

	/**
	 * Create a new material pre-configured for landscape use.
	 * Maps to action="create_landscape_material"
	 *
	 * @param MaterialName - Name for the new material
	 * @param DestinationPath - Path where to create the asset (e.g., "/Game/Materials")
	 * @return Create result with asset path
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static FLandscapeMaterialCreateResult CreateLandscapeMaterial(
		const FString& MaterialName,
		const FString& DestinationPath);

	// =================================================================
	// Layer Blend Node Management
	// =================================================================

	/**
	 * Create a LandscapeLayerBlend node in a material.
	 * Maps to action="create_layer_blend_node"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param PosX - X position in the material graph
	 * @param PosY - Y position in the material graph
	 * @return Blend node info with node ID
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static FLandscapeLayerBlendInfo CreateLayerBlendNode(
		const FString& MaterialPath,
		int32 PosX = -400,
		int32 PosY = 0);

	/**
	 * Create a LandscapeLayerBlend node with all layers pre-configured in one call.
	 * Maps to action="create_layer_blend_node_with_layers"
	 *
	 * Creates the blend node and adds all specified layers in a single transaction.
	 * Much faster than create_layer_blend_node + repeated add_layer_to_blend_node calls.
	 *
	 * @param MaterialPath - Full path to the material
	 * @param Layers - Array of layer configurations (name, blend type, preview weight)
	 * @param PosX - X position in the material graph
	 * @param PosY - Y position in the material graph
	 * @return Blend node info with node ID and all layers
	 *
	 * Example:
	 *   layers = [FLandscapeMaterialLayerConfig(LayerName="Grass", BlendType="LB_WeightBlend"),
	 *             FLandscapeMaterialLayerConfig(LayerName="Rock", BlendType="LB_WeightBlend")]
	 *   blend = unreal.LandscapeMaterialService.create_layer_blend_node_with_layers(mat_path, layers)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static FLandscapeLayerBlendInfo CreateLayerBlendNodeWithLayers(
		const FString& MaterialPath,
		const TArray<FLandscapeMaterialLayerConfig>& Layers,
		int32 PosX = -400,
		int32 PosY = 0);

	/**
	 * Add a layer to an existing LandscapeLayerBlend node.
	 * Maps to action="add_layer_to_blend_node"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param BlendNodeId - ID of the blend node expression
	 * @param LayerName - Name of the layer to add
	 * @param BlendType - Blend type: "LB_WeightBlend" (default), "LB_AlphaBlend", "LB_HeightBlend"
	 * @return True if layer was added
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static bool AddLayerToBlendNode(
		const FString& MaterialPath,
		const FString& BlendNodeId,
		const FString& LayerName,
		const FString& BlendType = TEXT("LB_WeightBlend"));

	/**
	 * Remove a layer from a LandscapeLayerBlend node.
	 * Maps to action="remove_layer_from_blend_node"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param BlendNodeId - ID of the blend node expression
	 * @param LayerName - Name of the layer to remove
	 * @return True if layer was removed
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static bool RemoveLayerFromBlendNode(
		const FString& MaterialPath,
		const FString& BlendNodeId,
		const FString& LayerName);

	/**
	 * Get information about all layers in a LandscapeLayerBlend node.
	 * Maps to action="get_layer_blend_info"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param BlendNodeId - ID of the blend node expression
	 * @return Blend info with all layer configurations
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static FLandscapeLayerBlendInfo GetLayerBlendInfo(
		const FString& MaterialPath,
		const FString& BlendNodeId);

	/**
	 * Connect an expression output to a specific layer input on a blend node.
	 * Maps to action="connect_to_layer_input"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param SourceExpressionId - Source expression ID
	 * @param SourceOutput - Output name (empty for first output)
	 * @param BlendNodeId - Blend node ID
	 * @param LayerName - Layer name to connect to
	 * @param InputType - Input type: "Layer" (diffuse color) or "Height" (for height blend)
	 * @return True if connection was made
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static bool ConnectToLayerInput(
		const FString& MaterialPath,
		const FString& SourceExpressionId,
		const FString& SourceOutput,
		const FString& BlendNodeId,
		const FString& LayerName,
		const FString& InputType = TEXT("Layer"));

	// =================================================================
	// Landscape Layer Coordinates
	// =================================================================

	/**
	 * Create a LandscapeLayerCoords expression for UV mapping.
	 * Maps to action="create_layer_coords_node"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param MappingScale - UV tiling scale (default 0.01 = tile every 100 units)
	 * @param PosX - X position in the material graph
	 * @param PosY - Y position in the material graph
	 * @return Expression ID of the created node, or empty string on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static FString CreateLayerCoordsNode(
		const FString& MaterialPath,
		float MappingScale = 0.01f,
		int32 PosX = -800,
		int32 PosY = 0);

	// =================================================================
	// Landscape Layer Sample Expression
	// =================================================================

	/**
	 * Create a LandscapeLayerSample expression to sample a layer's weight.
	 * Maps to action="create_layer_sample_node"
	 *
	 * Unlike LandscapeLayerBlend which blends multiple layers, LayerSample outputs
	 * the raw weight (0-1) of a single layer. Useful for masking and procedural effects.
	 *
	 * @param MaterialPath - Full path to the material
	 * @param LayerName - Name of the landscape layer to sample
	 * @param PosX - X position in the material graph
	 * @param PosY - Y position in the material graph
	 * @return Expression ID of the created node, or empty string on failure
	 *
	 * Example:
	 *   sample_id = unreal.LandscapeMaterialService.create_layer_sample_node("/Game/M_Test", "Grass", -800, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static FString CreateLayerSampleNode(
		const FString& MaterialPath,
		const FString& LayerName,
		int32 PosX = -800,
		int32 PosY = 0);

	// =================================================================
	// Landscape Grass Output
	// =================================================================

	/**
	 * Create a LandscapeGrassOutput expression for procedural grass/foliage spawning.
	 * Maps to action="create_grass_output"
	 *
	 * Each entry in GrassTypeNames maps a display name to a grass type asset path.
	 * The node will have one input per grass type for driving spawn density.
	 *
	 * @param MaterialPath - Full path to the material
	 * @param GrassTypeNames - Map of input name -> LandscapeGrassType asset path
	 * @param PosX - X position in the material graph
	 * @param PosY - Y position in the material graph
	 * @return Expression ID of the created node, or empty string on failure
	 *
	 * Example:
	 *   grass_id = unreal.LandscapeMaterialService.create_grass_output("/Game/M_Test",
	 *       {"ShortGrass": "/Game/Foliage/LGT_ShortGrass", "TallGrass": "/Game/Foliage/LGT_TallGrass"}, 400, 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static FString CreateGrassOutput(
		const FString& MaterialPath,
		const TMap<FString, FString>& GrassTypeNames,
		int32 PosX = 400,
		int32 PosY = 0);

	// =================================================================
	// Layer Info Object Management
	// =================================================================

	/**
	 * Create a ULandscapeLayerInfoObject asset.
	 * Maps to action="create_layer_info_object"
	 *
	 * @param LayerName - Name for the layer
	 * @param DestinationPath - Path where to create the asset (e.g., "/Game/Landscape")
	 * @param bIsWeightBlended - True for weight-blended (default), false for non-weight-blended
	 * @return Create result with asset path and layer name
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static FLandscapeLayerInfoCreateResult CreateLayerInfoObject(
		const FString& LayerName,
		const FString& DestinationPath,
		bool bIsWeightBlended = true);

	/**
	 * Get details about an existing layer info object.
	 * Maps to action="get_layer_info_details"
	 *
	 * @param LayerInfoAssetPath - Full path to the layer info asset
	 * @param OutLayerName - Layer name
	 * @param bOutIsWeightBlended - Whether it's weight blended
	 * @return True if found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static bool GetLayerInfoDetails(
		const FString& LayerInfoAssetPath,
		FString& OutLayerName,
		bool& bOutIsWeightBlended);

	// =================================================================
	// Material Assignment
	// =================================================================

	/**
	 * Assign a material to a landscape and configure layer info objects.
	 * Maps to action="assign_material_to_landscape"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape actor
	 * @param MaterialPath - Full path to the material asset
	 * @param LayerInfoPaths - Map of layer name -> layer info asset path
	 * @return True if assignment succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static bool AssignMaterialToLandscape(
		const FString& LandscapeNameOrLabel,
		const FString& MaterialPath,
		const TMap<FString, FString>& LayerInfoPaths);

	// =================================================================
	// Convenience Methods
	// =================================================================

	/**
	 * Set up a complete layer with diffuse texture, optional normal and roughness.
	 * Creates texture sample nodes and connects them to the blend node.
	 * Maps to action="setup_layer_textures"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param BlendNodeId - ID of the blend node
	 * @param LayerName - Layer name to configure
	 * @param DiffuseTexturePath - Path to the diffuse texture asset
	 * @param NormalTexturePath - Optional path to normal map texture
	 * @param RoughnessTexturePath - Optional path to roughness texture
	 * @param TextureTilingScale - UV tiling scale (default 0.01)
	 * @return True if setup succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static bool SetupLayerTextures(
		const FString& MaterialPath,
		const FString& BlendNodeId,
		const FString& LayerName,
		const FString& DiffuseTexturePath,
		const FString& NormalTexturePath = TEXT(""),
		const FString& RoughnessTexturePath = TEXT(""),
		float TextureTilingScale = 0.01f);

	// =================================================================
	// Landscape Layer Weight Expression
	// =================================================================

	/**
	 * Create a LandscapeLayerWeight expression (alternative to LandscapeLayerBlend).
	 * Maps to action="create_layer_weight_node"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param LayerName - Layer name for this weight node
	 * @param PreviewWeight - Preview weight value (0.0-1.0)
	 * @param PosX - X position in the material graph
	 * @param PosY - Y position in the material graph
	 * @return Expression ID of the created node, or empty string on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
	static FString CreateLayerWeightNode(
		const FString& MaterialPath,
		const FString& LayerName,
		float PreviewWeight = 1.0f,
		int32 PosX = -400,
		int32 PosY = 0);

	// =================================================================
	// Existence Checks
	// =================================================================

	/**
	 * Check if a material exists at the given path.
	 *
	 * @param MaterialPath - Full path to check
	 * @return True if material exists
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial|Exists")
	static bool LandscapeMaterialExists(const FString& MaterialPath);

	/**
	 * Check if a layer info object exists at the given path.
	 *
	 * @param LayerInfoAssetPath - Full path to check
	 * @return True if layer info object exists
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial|Exists")
	static bool LayerInfoExists(const FString& LayerInfoAssetPath);

private:
	static UMaterial* LoadMaterialAsset(const FString& MaterialPath);
	static UMaterialExpression* FindExpressionById(UMaterial* Material, const FString& ExpressionId);
	static FString GetExpressionId(UMaterialExpression* Expression);
	static UMaterialExpressionLandscapeLayerBlend* FindLayerBlendNode(UMaterial* Material, const FString& NodeId);
	static void RefreshMaterialGraph(UMaterial* Material);
};
