// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ULandscapeService.generated.h"

/**
 * Information about a landscape paint layer
 */
USTRUCT(BlueprintType)
struct FLandscapeLayerInfo_Custom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString LayerName;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString LayerInfoPath;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bIsWeightBlended = true;
};

/**
 * Comprehensive landscape information
 */
USTRUCT(BlueprintType)
struct FLandscapeInfo_Custom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString ActorName;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString ActorLabel;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FVector Scale = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 ComponentSizeQuads = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 SubsectionSizeQuads = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 NumSubsections = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 NumComponents = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 ResolutionX = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 ResolutionY = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString MaterialPath;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	TArray<FLandscapeLayerInfo_Custom> Layers;
};

/**
 * Result of landscape creation
 */
USTRUCT(BlueprintType)
struct FLandscapeCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString ActorLabel;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString ErrorMessage;
};

/**
 * Height sample at a world location
 */
USTRUCT(BlueprintType)
struct FLandscapeHeightSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	float Height = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bValid = false;
};

/**
 * Layer weight at a world location
 */
USTRUCT(BlueprintType)
struct FLandscapeLayerWeightSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString LayerName;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	float Weight = 0.0f;
};

/**
 * Result of applying noise to a landscape, including statistics about the operation.
 */
USTRUCT(BlueprintType)
struct FLandscapeNoiseResult
{
	GENERATED_BODY()

	/** Whether the noise was successfully applied */
	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bSuccess = false;

	/** Minimum height delta applied in world units */
	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	float MinDeltaApplied = 0.0f;

	/** Maximum height delta applied in world units */
	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	float MaxDeltaApplied = 0.0f;

	/** Number of vertices that were modified */
	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 VerticesModified = 0;

	/** Number of vertices that hit min/max height limit */
	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 SaturatedVertices = 0;

	/** Error message if bSuccess is false */
	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString ErrorMessage;
};

// =========================================================================
// Spline Data Structs
// =========================================================================

/** Information about a single landscape spline control point */
USTRUCT(BlueprintType)
struct FLandscapeSplinePointInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 PointIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	float Width = 500.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	float SideFalloff = 500.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	float EndFalloff = 500.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString LayerName;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bRaiseTerrain = true;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bLowerTerrain = true;
};

/** Information about a single landscape spline segment */
USTRUCT(BlueprintType)
struct FLandscapeSplineSegmentInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 SegmentIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 StartPointIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 EndPointIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	float StartTangentLength = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	float EndTangentLength = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString LayerName;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bRaiseTerrain = true;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bLowerTerrain = true;
};

/** Complete spline state on a landscape */
USTRUCT(BlueprintType)
struct FLandscapeSplineInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 NumControlPoints = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 NumSegments = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	TArray<FLandscapeSplinePointInfo> ControlPoints;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	TArray<FLandscapeSplineSegmentInfo> Segments;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString ErrorMessage;
};

/** Result of creating a spline control point */
USTRUCT(BlueprintType)
struct FSplineCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 PointIndex = -1;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString ErrorMessage;
};

// =========================================================================
// Weight Map Data Structs
// =========================================================================

/** Result of exporting a weight map */
USTRUCT(BlueprintType)
struct FWeightMapExportResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString FilePath;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 Width = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 Height = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString ErrorMessage;
};

/** Result of importing a weight map */
USTRUCT(BlueprintType)
struct FWeightMapImportResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	int32 VerticesModified = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Landscape")
	FString ErrorMessage;
};

/**
 * Landscape service exposed directly to Python.
 *
 * Provides 44 landscape management actions:
 *
 * Discovery:
 * - list_landscapes: List all landscapes in the level
 * - get_landscape_info: Get detailed landscape information
 *
 * Lifecycle:
 * - create_landscape: Create a new landscape with configurable dimensions
 * - delete_landscape: Remove a landscape from the level
 *
 * Heightmap:
 * - import_heightmap: Import heightmap from PNG (preferred) or RAW file
 * - export_heightmap: Export heightmap to PNG (default) or RAW file
 * - get_height_at_location: Sample height at world position
 * - get_height_in_region: Read heights in a rectangular region (bulk read)
 * - set_height_in_region: Set heights in a rectangular region
 *
 * Sculpting:
 * - sculpt_at_location: Raise/lower terrain with brush (world-space height delta)
 * - flatten_at_location: Flatten terrain to target height
 * - smooth_at_location: Smooth terrain with brush
 * - raise_lower_region: Raise/lower a rectangular world-space region
 * - apply_noise: Apply procedural noise for natural terrain
 *
 * Layers:
 * - list_layers: List paint layers on landscape
 * - add_layer: Add a paint layer
 * - remove_layer: Remove a paint layer
 * - get_layer_weights_at_location: Get layer weights at position
 * - paint_layer_at_location: Paint a layer with brush
 *
 * Properties:
 * - set_landscape_material: Assign material to landscape
 * - get_landscape_property: Get a property value
 * - set_landscape_property: Set a property value
 *
 * Visibility:
 * - set_landscape_visibility: Show/hide landscape
 * - set_landscape_collision: Enable/disable collision
 *
 * Existence:
 * - landscape_exists: Check if landscape exists
 * - layer_exists: Check if layer exists on landscape
 *
 * Python Usage:
 *   import unreal
 *
 *   # Create a landscape
 *   result = unreal.LandscapeService.create_landscape(
 *       unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), unreal.Vector(100, 100, 100))
 *
 *   # List landscapes
 *   landscapes = unreal.LandscapeService.list_landscapes()
 *
 *   # Get height at location
 *   sample = unreal.LandscapeService.get_height_at_location("Landscape", 500.0, 500.0)
 *
 *   # Sculpt a mountain (raise by 5000 world units)
 *   unreal.LandscapeService.sculpt_at_location("Landscape", 0.0, 0.0, 5000.0, 5000.0, "Smooth")
 *
 *   # Raise a rectangular region by 1000 world units
 *   unreal.LandscapeService.raise_lower_region("Landscape", 0.0, 0.0, 5000.0, 5000.0, 1000.0)
 *
 *   # Apply procedural noise for natural terrain
 *   unreal.LandscapeService.apply_noise("Landscape", 0.0, 0.0, 10000.0, 500.0, 0.005, 42)
 */
UCLASS(BlueprintType)
class VIBEUE_API ULandscapeService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Discovery Operations
	// =================================================================

	/**
	 * List all landscape actors in the current level.
	 * Maps to action="list_landscapes"
	 *
	 * @return Array of landscape information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static TArray<FLandscapeInfo_Custom> ListLandscapes();

	/**
	 * Get detailed information about a landscape.
	 * Maps to action="get_landscape_info"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape actor
	 * @param OutInfo - Structure containing landscape details
	 * @return True if landscape found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool GetLandscapeInfo(const FString& LandscapeNameOrLabel, FLandscapeInfo_Custom& OutInfo);

	// =================================================================
	// Lifecycle Operations
	// =================================================================

	/**
	 * Create a new landscape with specified dimensions.
	 * Maps to action="create_landscape"
	 *
	 * @param Location - World location for the landscape
	 * @param Rotation - World rotation
	 * @param Scale - Scale (default 100,100,100 = 1m per unit)
	 * @param SectionsPerComponent - Sections per component (1 or 2)
	 * @param QuadsPerSection - Quads per section (7, 15, 31, 63, 127, or 255)
	 * @param ComponentCountX - Number of components in X direction
	 * @param ComponentCountY - Number of components in Y direction
	 * @param LandscapeLabel - Optional display label
	 * @return Create result with actor label
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static FLandscapeCreateResult CreateLandscape(
		FVector Location,
		FRotator Rotation,
		FVector Scale,
		int32 SectionsPerComponent = 1,
		int32 QuadsPerSection = 63,
		int32 ComponentCountX = 8,
		int32 ComponentCountY = 8,
		const FString& LandscapeLabel = TEXT(""));

	/**
	 * Delete a landscape from the level.
	 * Maps to action="delete_landscape"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape to delete
	 * @return True if deleted successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool DeleteLandscape(const FString& LandscapeNameOrLabel);

	// =================================================================
	// Heightmap Operations
	// =================================================================

	/**
	 * Import a heightmap from a 16-bit PNG (preferred) or RAW file.
	 * Maps to action="import_heightmap"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param FilePath - Absolute path to the heightmap file
	 * @return True if import succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool ImportHeightmap(
		const FString& LandscapeNameOrLabel,
		const FString& FilePath);

	/**
	 * Export a heightmap to a 16-bit PNG file by default (or RAW if a non-PNG extension is provided).
	 * Maps to action="export_heightmap"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param OutputFilePath - Absolute path for the output file
	 * @return True if export succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool ExportHeightmap(
		const FString& LandscapeNameOrLabel,
		const FString& OutputFilePath);

	/**
	 * Get the height at a specific world XY location.
	 * Maps to action="get_height_at_location"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param WorldX - World X coordinate
	 * @param WorldY - World Y coordinate
	 * @return Height sample with value and validity
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static FLandscapeHeightSample GetHeightAtLocation(
		const FString& LandscapeNameOrLabel,
		float WorldX, float WorldY);

	/**
	 * Get height values in a rectangular region using landscape-local vertex indices.
	 * Maps to action="get_height_in_region"
	 *
	 * Returns world-space Z heights for each vertex in the region.
	 * The array is row-major (SizeX values per row, SizeY rows).
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param StartX - Start X vertex index
	 * @param StartY - Start Y vertex index
	 * @param SizeX - Width in vertices
	 * @param SizeY - Height in vertices
	 * @return Row-major array of world-space Z heights (size = SizeX * SizeY), empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static TArray<float> GetHeightInRegion(
		const FString& LandscapeNameOrLabel,
		int32 StartX, int32 StartY,
		int32 SizeX, int32 SizeY);

	/**
	 * Set height values in a rectangular region using landscape-local vertex indices.
	 * Maps to action="set_height_in_region"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param StartX - Start X vertex index
	 * @param StartY - Start Y vertex index
	 * @param SizeX - Width in vertices
	 * @param SizeY - Height in vertices
	 * @param Heights - Row-major array of height values (size must equal SizeX * SizeY)
	 * @return True if heights were set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool SetHeightInRegion(
		const FString& LandscapeNameOrLabel,
		int32 StartX, int32 StartY,
		int32 SizeX, int32 SizeY,
		const TArray<float>& Heights);

	// =================================================================
	// Sculpting Operations
	// =================================================================

	/**
	 * Raise or lower terrain at a world location.
	 * Maps to action="sculpt_at_location"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param WorldX - World X coordinate
	 * @param WorldY - World Y coordinate
	 * @param BrushRadius - Brush radius in world units
	 * @param Strength - Height delta in world units (positive=raise, negative=lower). E.g. 500.0 raises by ~500 units at center.
	 * @param BrushFalloffType - Falloff type: "Linear", "Smooth", "Spherical", "Tip"
	 * @return True if sculpting succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool SculptAtLocation(
		const FString& LandscapeNameOrLabel,
		float WorldX, float WorldY,
		float BrushRadius,
		float Strength,
		const FString& BrushFalloffType = TEXT("Linear"));

	/**
	 * Flatten terrain at a world location to a target height.
	 * Maps to action="flatten_at_location"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param WorldX - World X coordinate
	 * @param WorldY - World Y coordinate
	 * @param BrushRadius - Brush radius in world units
	 * @param TargetHeight - Height to flatten towards (world Z)
	 * @param Strength - Blend strength (0.0-1.0)
	 * @param BrushFalloffType - Falloff type: "Linear", "Smooth", "Spherical", "Tip"
	 * @return True if flatten succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool FlattenAtLocation(
		const FString& LandscapeNameOrLabel,
		float WorldX, float WorldY,
		float BrushRadius,
		float TargetHeight,
		float Strength = 1.0f,
		const FString& BrushFalloffType = TEXT("Smooth"));

	/**
	 * Smooth terrain at a world location using adaptive Gaussian blur.
	 * Maps to action="smooth_at_location"
	 *
	 * Uses a variable-radius Gaussian kernel that scales with brush radius
	 * and strength. Larger brushes and higher strength produce stronger
	 * smoothing that can bridge cliffs and transition zones.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param WorldX - World X coordinate
	 * @param WorldY - World Y coordinate
	 * @param BrushRadius - Brush radius in world units
	 * @param Strength - Smoothing strength (0.0-1.0). Also controls kernel size.
	 * @param BrushFalloffType - Falloff type: "Linear", "Smooth", "Spherical", "Tip"
	 * @return True if smoothing succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool SmoothAtLocation(
		const FString& LandscapeNameOrLabel,
		float WorldX, float WorldY,
		float BrushRadius,
		float Strength = 0.5f,
		const FString& BrushFalloffType = TEXT("Smooth"));

	/**
	 * Raise or lower all terrain in a world-space rectangular region by a height delta.
	 * Maps to action="raise_lower_region"
	 *
	 * Unlike sculpt_at_location (circular brush with falloff), this modifies
	 * every vertex in the rectangle. Use FalloffWidth for gradual edge transitions.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param WorldCenterX - Center X of the region in world units
	 * @param WorldCenterY - Center Y of the region in world units
	 * @param WorldWidth - Full width of the region in world units
	 * @param WorldHeight - Full height of the region in world units
	 * @param HeightDelta - Height change in world units (positive=raise, negative=lower)
	 * @param FalloffWidth - Width of the smooth falloff band at edges in world units (default 0 = hard edges)
	 * @return True if operation succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool RaiseLowerRegion(
		const FString& LandscapeNameOrLabel,
		float WorldCenterX, float WorldCenterY,
		float WorldWidth, float WorldHeight,
		float HeightDelta,
		float FalloffWidth = 0.0f);

	/**
	 * Apply procedural noise to terrain for natural-looking variation.
	 * Maps to action="apply_noise"
	 *
	 * Generates Perlin-like noise and adds it to the existing heightmap
	 * within a circular region. Returns statistics about the operation
	 * including min/max deltas and saturation info.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param WorldCenterX - Center X of the noise region in world units
	 * @param WorldCenterY - Center Y of the noise region in world units
	 * @param WorldRadius - Radius of the affected region in world units
	 * @param Amplitude - Maximum height variation in world units (e.g. 500 = +/-250 variation)
	 * @param Frequency - Noise frequency. Higher = more detail. Typical: 0.001-0.01
	 * @param Seed - Random seed for reproducible results
	 * @param Octaves - Number of noise octaves (1-8). Higher = more detail. Default 4.
	 * @return Noise result with statistics (min/max delta, vertices modified, saturation count)
	 *
	 * Python Usage:
	 *   result = unreal.LandscapeService.apply_noise("Landscape", 0.0, 0.0, 10000.0, 500.0, 0.005, 42, 4)
	 *   print(f"Delta range: [{result.min_delta_applied}, {result.max_delta_applied}]")
	 *   print(f"Vertices: {result.vertices_modified}, Saturated: {result.saturated_vertices}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static FLandscapeNoiseResult ApplyNoise(
		const FString& LandscapeNameOrLabel,
		float WorldCenterX, float WorldCenterY,
		float WorldRadius,
		float Amplitude,
		float Frequency = 0.005f,
		int32 Seed = 0,
		int32 Octaves = 4);

	// =================================================================
	// Paint Layer Operations
	// =================================================================

	/**
	 * List all paint layers on a landscape.
	 * Maps to action="list_layers"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @return Array of layer information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static TArray<FLandscapeLayerInfo_Custom> ListLayers(
		const FString& LandscapeNameOrLabel);

	/**
	 * Add a paint layer to a landscape.
	 * Maps to action="add_layer"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param LayerInfoAssetPath - Path to the ULandscapeLayerInfoObject asset
	 * @return True if layer was added
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool AddLayer(
		const FString& LandscapeNameOrLabel,
		const FString& LayerInfoAssetPath);

	/**
	 * Remove a paint layer from a landscape.
	 * Maps to action="remove_layer"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param LayerName - Name of the layer to remove
	 * @return True if layer was removed
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool RemoveLayer(
		const FString& LandscapeNameOrLabel,
		const FString& LayerName);

	/**
	 * Get layer weights at a world location.
	 * Maps to action="get_layer_weights_at_location"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param WorldX - World X coordinate
	 * @param WorldY - World Y coordinate
	 * @return Array of layer weight samples
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static TArray<FLandscapeLayerWeightSample> GetLayerWeightsAtLocation(
		const FString& LandscapeNameOrLabel,
		float WorldX, float WorldY);

	/**
	 * Paint a layer at a world location using a brush.
	 * Maps to action="paint_layer_at_location"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param LayerName - Name of the layer to paint
	 * @param WorldX - World X coordinate
	 * @param WorldY - World Y coordinate
	 * @param BrushRadius - Brush radius in world units
	 * @param Strength - Paint strength (0.0-1.0)
	 * @return True if painting succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool PaintLayerAtLocation(
		const FString& LandscapeNameOrLabel,
		const FString& LayerName,
		float WorldX, float WorldY,
		float BrushRadius,
		float Strength = 1.0f);

	// =================================================================
	// Property Operations
	// =================================================================

	/**
	 * Set the material on a landscape.
	 * Maps to action="set_landscape_material"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param MaterialPath - Full path to the material asset
	 * @return True if material was assigned
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool SetLandscapeMaterial(
		const FString& LandscapeNameOrLabel,
		const FString& MaterialPath);

	/**
	 * Get a landscape property value.
	 * Maps to action="get_landscape_property"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param PropertyName - Property name
	 * @return Property value as string
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static FString GetLandscapeProperty(
		const FString& LandscapeNameOrLabel,
		const FString& PropertyName);

	/**
	 * Set a landscape property value.
	 * Maps to action="set_landscape_property"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param PropertyName - Property name
	 * @param Value - Value as string
	 * @return True if property was set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool SetLandscapeProperty(
		const FString& LandscapeNameOrLabel,
		const FString& PropertyName,
		const FString& Value);

	// =================================================================
	// Visibility & Collision
	// =================================================================

	/**
	 * Set visibility of a landscape.
	 * Maps to action="set_landscape_visibility"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param bVisible - Whether the landscape should be visible
	 * @return True if visibility was set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool SetLandscapeVisibility(
		const FString& LandscapeNameOrLabel,
		bool bVisible);

	/**
	 * Enable or disable collision on a landscape.
	 * Maps to action="set_landscape_collision"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param bEnableCollision - Whether collision should be enabled
	 * @return True if collision setting was changed
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static bool SetLandscapeCollision(
		const FString& LandscapeNameOrLabel,
		bool bEnableCollision);

	// =================================================================
	// Existence Checks
	// =================================================================

	/**
	 * Check if a landscape exists with the given name or label.
	 *
	 * @param LandscapeNameOrLabel - Name or label to search for
	 * @return True if landscape exists
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Exists")
	static bool LandscapeExists(const FString& LandscapeNameOrLabel);

	/**
	 * Check if a layer exists on a landscape.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param LayerName - Layer name to check
	 * @return True if layer exists
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Exists")
	static bool LayerExists(
		const FString& LandscapeNameOrLabel,
		const FString& LayerName);

	// =================================================================
	// Batch Painting Operations
	// =================================================================

	/**
	 * Paint a layer across a rectangular vertex-index region with uniform weight.
	 * Maps to action="paint_layer_in_region"
	 *
	 * Much faster than calling paint_layer_at_location in a loop. Uses bulk
	 * TAlphamapAccessor write — a single SetData+Flush covers the whole region.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param LayerName - Paint layer to write
	 * @param StartX - First vertex X index (inclusive)
	 * @param StartY - First vertex Y index (inclusive)
	 * @param SizeX - Region width in vertices
	 * @param SizeY - Region height in vertices
	 * @param Strength - Target weight (0.0–1.0). Each vertex is set to this value.
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
	static bool PaintLayerInRegion(
		const FString& LandscapeNameOrLabel,
		const FString& LayerName,
		int32 StartX, int32 StartY,
		int32 SizeX, int32 SizeY,
		float Strength = 1.0f);

	/**
	 * Paint a layer across a world-space rectangular region with uniform weight.
	 * Maps to action="paint_layer_in_world_rect"
	 *
	 * Converts world coordinates to vertex indices and delegates to PaintLayerInRegion.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param LayerName - Paint layer to write
	 * @param WorldMinX - Minimum X world coordinate
	 * @param WorldMinY - Minimum Y world coordinate
	 * @param WorldMaxX - Maximum X world coordinate
	 * @param WorldMaxY - Maximum Y world coordinate
	 * @param Strength - Target weight (0.0–1.0)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
	static bool PaintLayerInWorldRect(
		const FString& LandscapeNameOrLabel,
		const FString& LayerName,
		float WorldMinX, float WorldMinY,
		float WorldMaxX, float WorldMaxY,
		float Strength = 1.0f);

	// =================================================================
	// Weight Map Import / Export
	// =================================================================

	/**
	 * Export a single paint layer's weight data as an 8-bit grayscale PNG.
	 * Maps to action="export_weight_map"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param LayerName - Layer to export (e.g. "L1")
	 * @param OutputFilePath - Absolute path for the output .png file
	 * @return Export result with dimensions and error info
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
	static FWeightMapExportResult ExportWeightMap(
		const FString& LandscapeNameOrLabel,
		const FString& LayerName,
		const FString& OutputFilePath);

	/**
	 * Import layer weights from an 8-bit grayscale PNG (R channel is used).
	 * Maps to action="import_weight_map"
	 *
	 * Image dimensions must match the landscape vertex resolution exactly.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param LayerName - Target paint layer
	 * @param FilePath - Absolute path to source PNG file
	 * @return Import result with vertex count and error info
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
	static FWeightMapImportResult ImportWeightMap(
		const FString& LandscapeNameOrLabel,
		const FString& LayerName,
		const FString& FilePath);

	/**
	 * Bulk-read layer weights as a flat float array (0.0–1.0), row-major.
	 * Maps to action="get_weights_in_region"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param LayerName - Layer to read
	 * @param StartX - Start vertex X
	 * @param StartY - Start vertex Y
	 * @param SizeX - Width in vertices
	 * @param SizeY - Height in vertices
	 * @return Row-major float array (size = SizeX * SizeY), empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
	static TArray<float> GetWeightsInRegion(
		const FString& LandscapeNameOrLabel,
		const FString& LayerName,
		int32 StartX, int32 StartY,
		int32 SizeX, int32 SizeY);

	/**
	 * Bulk-write layer weights from a flat float array (0.0–1.0), row-major.
	 * Maps to action="set_weights_in_region"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param LayerName - Target paint layer
	 * @param StartX - Start vertex X
	 * @param StartY - Start vertex Y
	 * @param SizeX - Width in vertices
	 * @param SizeY - Height in vertices
	 * @param Weights - Values 0.0–1.0, size must equal SizeX * SizeY
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
	static bool SetWeightsInRegion(
		const FString& LandscapeNameOrLabel,
		const FString& LayerName,
		int32 StartX, int32 StartY,
		int32 SizeX, int32 SizeY,
		const TArray<float>& Weights);

	// =================================================================
	// Landscape Holes (Visibility Mask)
	// =================================================================

	/**
	 * Punch or fill a circular hole at a world position.
	 * Maps to action="set_hole_at_location"
	 *
	 * Uses the landscape visibility layer (weight 255 = hole, 0 = solid).
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param WorldX - Center X coordinate in world units
	 * @param WorldY - Center Y coordinate in world units
	 * @param BrushRadius - Radius of hole in world units
	 * @param bCreateHole - True to punch a hole, False to fill
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Holes")
	static bool SetHoleAtLocation(
		const FString& LandscapeNameOrLabel,
		float WorldX, float WorldY,
		float BrushRadius,
		bool bCreateHole = true);

	/**
	 * Set hole visibility for a rectangular region of vertices.
	 * Maps to action="set_hole_in_region"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param StartX - Start vertex X
	 * @param StartY - Start vertex Y
	 * @param SizeX - Width in vertices
	 * @param SizeY - Height in vertices
	 * @param bCreateHole - True to punch a hole, False to fill
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Holes")
	static bool SetHoleInRegion(
		const FString& LandscapeNameOrLabel,
		int32 StartX, int32 StartY,
		int32 SizeX, int32 SizeY,
		bool bCreateHole = true);

	/**
	 * Check if a world position is inside a hole.
	 * Maps to action="get_hole_at_location"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param WorldX - World X coordinate
	 * @param WorldY - World Y coordinate
	 * @return True if the vertex at that location is a hole
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Holes")
	static bool GetHoleAtLocation(
		const FString& LandscapeNameOrLabel,
		float WorldX, float WorldY);

	// =================================================================
	// Landscape Splines
	// =================================================================

	/**
	 * Create a new spline control point on the landscape.
	 * Maps to action="create_spline_point"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param WorldLocation - World-space position for the control point
	 * @param Width - Half-width of the spline influence (world units)
	 * @param SideFalloff - Falloff at the sides (world units)
	 * @param EndFalloff - Falloff at start/end tips (world units)
	 * @param PaintLayerName - Paint layer applied under the spline (empty = none)
	 * @param bRaiseTerrain - Whether to raise terrain to spline level
	 * @param bLowerTerrain - Whether to lower terrain to spline level
	 * @return Create result with index of the new point
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
	static FSplineCreateResult CreateSplinePoint(
		const FString& LandscapeNameOrLabel,
		FVector WorldLocation,
		float Width = 500.0f,
		float SideFalloff = 500.0f,
		float EndFalloff = 500.0f,
		const FString& PaintLayerName = TEXT(""),
		bool bRaiseTerrain = true,
		bool bLowerTerrain = true);

	/**
	 * Connect two existing control points with a spline segment.
	 * Maps to action="connect_spline_points"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param StartPointIndex - Index of the start control point
	 * @param EndPointIndex - Index of the end control point
	 * @param TangentLength - Tangent arm length (0 = auto from distance)
	 * @param PaintLayerName - Layer painted under this segment
	 * @param bRaiseTerrain - Whether to raise terrain under segment
	 * @param bLowerTerrain - Whether to lower terrain under segment
	 * @return True if segment was created
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
	static bool ConnectSplinePoints(
		const FString& LandscapeNameOrLabel,
		int32 StartPointIndex,
		int32 EndPointIndex,
		float TangentLength = 0.0f,
		const FString& PaintLayerName = TEXT(""),
		bool bRaiseTerrain = true,
		bool bLowerTerrain = true);

	/**
	 * Create an entire spline path from an array of world locations.
	 * Maps to action="create_spline_from_points"
	 *
	 * Creates control points and connects them sequentially. This is the
	 * "just draw me a road" convenience action.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param WorldLocations - Ordered world-space positions along the path
	 * @param Width - Half-width for all control points
	 * @param SideFalloff - Side falloff for all control points
	 * @param EndFalloff - End falloff for all control points
	 * @param PaintLayerName - Layer painted under the entire spline
	 * @param bRaiseTerrain - Raise terrain along spline
	 * @param bLowerTerrain - Lower terrain along spline
	 * @param bClosedLoop - Whether to connect last point back to first
	 * @return Spline info describing all created points and segments
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
	static FLandscapeSplineInfo CreateSplineFromPoints(
		const FString& LandscapeNameOrLabel,
		const TArray<FVector>& WorldLocations,
		float Width = 500.0f,
		float SideFalloff = 500.0f,
		float EndFalloff = 500.0f,
		const FString& PaintLayerName = TEXT(""),
		bool bRaiseTerrain = true,
		bool bLowerTerrain = true,
		bool bClosedLoop = false);

	/**
	 * Get all control points and segments on a landscape's spline component.
	 * Maps to action="get_spline_info"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @return Spline info with all control points and segments
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
	static FLandscapeSplineInfo GetSplineInfo(
		const FString& LandscapeNameOrLabel);

	/**
	 * Modify an existing control point's properties.
	 * Maps to action="modify_spline_point"
	 *
	 * Pass -1.0 for Width/SideFalloff/EndFalloff to leave them unchanged.
	 * Pass "__unchanged__" for PaintLayerName to leave it unchanged.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param PointIndex - Index of the control point to modify
	 * @param WorldLocation - New world-space location
	 * @param Width - New half-width (-1 = unchanged)
	 * @param SideFalloff - New side falloff (-1 = unchanged)
	 * @param EndFalloff - New end falloff (-1 = unchanged)
	 * @param PaintLayerName - New paint layer name ("__unchanged__" = no change)
	 * @return True if modified successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
	static bool ModifySplinePoint(
		const FString& LandscapeNameOrLabel,
		int32 PointIndex,
		FVector WorldLocation,
		float Width = -1.0f,
		float SideFalloff = -1.0f,
		float EndFalloff = -1.0f,
		const FString& PaintLayerName = TEXT("__unchanged__"));

	/**
	 * Remove a control point and all its connected segments.
	 * Maps to action="delete_spline_point"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param PointIndex - Index of the control point to remove
	 * @return True if deleted successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
	static bool DeleteSplinePoint(
		const FString& LandscapeNameOrLabel,
		int32 PointIndex);

	/**
	 * Clear all control points and segments from the landscape.
	 * Maps to action="delete_all_splines"
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @return True if cleared successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
	static bool DeleteAllSplines(
		const FString& LandscapeNameOrLabel);

	/**
	 * Apply terrain deformation and layer painting for all splines.
	 * Maps to action="apply_splines_to_landscape"
	 *
	 * Triggers UE's built-in spline → landscape rasterization, which raises/lowers
	 * terrain and paints weight layers under all spline segments.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @return True if applied successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
	static bool ApplySplinesToLandscape(
		const FString& LandscapeNameOrLabel);

	// =================================================================
	// Landscape Resize
	// =================================================================

	/**
	 * Resize a landscape to a new component grid, resampling height and weight data.
	 * Maps to action="resize_landscape"
	 *
	 * WARNING: This is a destructive operation. The old landscape is deleted and
	 * recreated with new dimensions. Foliage instances are NOT transferred.
	 * Splines are also NOT transferred — rebuild them after resizing.
	 *
	 * Implementation uses export → delete → create → bilinear-resample → reimport.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the landscape to resize
	 * @param NewComponentCountX - New number of components in X
	 * @param NewComponentCountY - New number of components in Y
	 * @param NewQuadsPerSection - Quads per section (-1 = keep current)
	 * @param NewSectionsPerComponent - Sections per component (-1 = keep current)
	 * @return Create result describing the new landscape
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
	static FLandscapeCreateResult ResizeLandscape(
		const FString& LandscapeNameOrLabel,
		int32 NewComponentCountX,
		int32 NewComponentCountY,
		int32 NewQuadsPerSection = -1,
		int32 NewSectionsPerComponent = -1);

private:
	static class ALandscape* FindLandscapeByIdentifier(const FString& NameOrLabel);
	static class UWorld* GetEditorWorld();
	static class ULandscapeInfo* GetLandscapeInfoForActor(class ALandscapeProxy* Landscape);
	static void PopulateLandscapeInfo(class ALandscapeProxy* Landscape, FLandscapeInfo_Custom& OutInfo);
	static void UpdateLandscapeAfterHeightEdit(class ALandscapeProxy* Landscape);
};
