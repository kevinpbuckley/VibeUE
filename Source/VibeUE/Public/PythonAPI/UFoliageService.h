// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UFoliageService.generated.h"

/**
 * Single foliage instance info (for queries and specific placement)
 */
USTRUCT(BlueprintType)
struct FFoliageInstanceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	FVector Scale = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	int32 InstanceIndex = -1;
};

/**
 * Result from adding/scattering foliage
 */
USTRUCT(BlueprintType)
struct FFoliageScatterResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	int32 InstancesAdded = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	int32 InstancesRequested = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	int32 InstancesRejected = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	FString ErrorMessage;
};

/**
 * Result from removing foliage
 */
USTRUCT(BlueprintType)
struct FFoliageRemoveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	int32 InstancesRemoved = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	FString ErrorMessage;
};

/**
 * Info about a foliage type in the level
 */
USTRUCT(BlueprintType)
struct FVibeUEFoliageTypeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	FString FoliageTypeName;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	FString MeshPath;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	int32 InstanceCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	FString FoliageTypePath;
};

/**
 * Result from creating a foliage type asset
 */
USTRUCT(BlueprintType)
struct FFoliageTypeCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	FString ErrorMessage;
};

/**
 * Query result for foliage instances
 */
USTRUCT(BlueprintType)
struct FFoliageQueryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	int32 TotalInstances = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	TArray<FFoliageInstanceInfo> Instances;

	UPROPERTY(BlueprintReadWrite, Category = "Foliage")
	FString ErrorMessage;
};

/**
 * Foliage service exposed directly to Python.
 *
 * Provides foliage placement and management for landscapes:
 *
 * Discovery:
 * - list_foliage_types: List all foliage types in the level with instance counts
 * - get_instance_count: Get instance count for a specific foliage type
 *
 * Foliage Type Management:
 * - create_foliage_type: Create a UFoliageType asset from a static mesh
 * - set_foliage_type_property: Set a property on a foliage type asset
 * - get_foliage_type_property: Get a property from a foliage type asset
 *
 * Placement:
 * - scatter_foliage: Scatter instances in a circular region with Poisson disk sampling
 * - scatter_foliage_rect: Scatter instances in a rectangular region
 * - add_foliage_instances: Place instances at specific locations
 *
 * Layer-Aware Placement:
 * - scatter_foliage_on_layer: Scatter only where a landscape paint layer is dominant
 *
 * Removal:
 * - remove_foliage_in_radius: Remove instances of a type in a circular region
 * - remove_all_foliage_of_type: Remove all instances of a type from the level
 * - clear_all_foliage: Remove all foliage of all types
 *
 * Query:
 * - get_foliage_in_radius: Get foliage instances in a circular region
 *
 * Existence:
 * - foliage_type_exists: Check if a foliage type asset exists
 * - has_foliage_instances: Check if any foliage instances exist for a type
 *
 * Python Usage:
 *   import unreal
 *
 *   # Scatter 200 trees
 *   result = unreal.FoliageService.scatter_foliage(
 *       "/Game/Meshes/SM_Tree", 0.0, 0.0, 5000.0, 200)
 *
 *   # List foliage types
 *   types = unreal.FoliageService.list_foliage_types()
 *
 *   # Remove trees in area
 *   unreal.FoliageService.remove_foliage_in_radius("/Game/Meshes/SM_Tree", 0.0, 0.0, 1000.0)
 */
UCLASS(BlueprintType)
class VIBEUE_API UFoliageService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Discovery
	// =================================================================

	/**
	 * List all foliage types currently in the level with instance counts.
	 *
	 * @return Array of foliage type information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static TArray<FVibeUEFoliageTypeInfo> ListFoliageTypes();

	/**
	 * Get instance count for a specific mesh or foliage type in the level.
	 *
	 * @param MeshOrFoliageTypePath - Path to UStaticMesh or UFoliageType asset
	 * @return Number of instances, or -1 if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static int32 GetInstanceCount(const FString& MeshOrFoliageTypePath);

	// =================================================================
	// Foliage Type Management
	// =================================================================

	/**
	 * Create a UFoliageType asset from a static mesh with configurable defaults.
	 *
	 * @param MeshPath - Path to the UStaticMesh asset
	 * @param SavePath - Directory to save the foliage type (e.g. "/Game/Foliage")
	 * @param AssetName - Name for the foliage type asset (e.g. "FT_PineTree")
	 * @param MinScale - Minimum random scale (default 0.8)
	 * @param MaxScale - Maximum random scale (default 1.2)
	 * @param bAlignToNormal - Align instances to surface normal (default true)
	 * @param AlignToNormalMaxAngle - Max angle for normal alignment in degrees (default 45)
	 * @param GroundSlopeMaxAngle - Max ground slope for placement in degrees (default 45)
	 * @param CullDistanceMax - Max cull distance in world units (default 20000)
	 * @return Create result with asset path
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static FFoliageTypeCreateResult CreateFoliageType(
		const FString& MeshPath,
		const FString& SavePath,
		const FString& AssetName,
		float MinScale = 0.8f,
		float MaxScale = 1.2f,
		bool bAlignToNormal = true,
		float AlignToNormalMaxAngle = 45.0f,
		float GroundSlopeMaxAngle = 45.0f,
		float CullDistanceMax = 20000.0f);

	/**
	 * Set a property on an existing foliage type asset.
	 *
	 * @param FoliageTypePath - Path to the UFoliageType asset
	 * @param PropertyName - Property name (e.g. "Scaling", "CullDistance.Max")
	 * @param Value - Value as string
	 * @return True if property was set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static bool SetFoliageTypeProperty(
		const FString& FoliageTypePath,
		const FString& PropertyName,
		const FString& Value);

	/**
	 * Get a property from an existing foliage type asset.
	 *
	 * @param FoliageTypePath - Path to the UFoliageType asset
	 * @param PropertyName - Property name
	 * @return Property value as string, empty if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static FString GetFoliageTypeProperty(
		const FString& FoliageTypePath,
		const FString& PropertyName);

	// =================================================================
	// Placement
	// =================================================================

	/**
	 * Scatter foliage instances in a circular region using Poisson disk sampling.
	 * Traces to surface for height and optional normal alignment.
	 *
	 * @param MeshOrFoliageTypePath - Path to UStaticMesh or UFoliageType asset
	 * @param WorldCenterX - Center X of scatter region
	 * @param WorldCenterY - Center Y of scatter region
	 * @param Radius - Radius of scatter region in world units
	 * @param Count - Target number of instances
	 * @param MinScale - Minimum random scale (default 0.8)
	 * @param MaxScale - Maximum random scale (default 1.2)
	 * @param bAlignToNormal - Align to surface normal (default true)
	 * @param bRandomYaw - Apply random yaw rotation (default true)
	 * @param Seed - Random seed for reproducibility (0 = random)
	 * @param LandscapeNameOrLabel - Optional landscape to constrain placement to
	 * @return Scatter result with instance counts
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static FFoliageScatterResult ScatterFoliage(
		const FString& MeshOrFoliageTypePath,
		float WorldCenterX,
		float WorldCenterY,
		float Radius,
		int32 Count,
		float MinScale = 0.8f,
		float MaxScale = 1.2f,
		bool bAlignToNormal = true,
		bool bRandomYaw = true,
		int32 Seed = 0,
		const FString& LandscapeNameOrLabel = TEXT(""));

	/**
	 * Scatter foliage instances in a rectangular region.
	 *
	 * @param MeshOrFoliageTypePath - Path to UStaticMesh or UFoliageType asset
	 * @param WorldMinX - Min X of rectangle
	 * @param WorldMinY - Min Y of rectangle
	 * @param WorldMaxX - Max X of rectangle
	 * @param WorldMaxY - Max Y of rectangle
	 * @param Count - Target number of instances
	 * @param MinScale - Minimum random scale (default 0.8)
	 * @param MaxScale - Maximum random scale (default 1.2)
	 * @param bAlignToNormal - Align to surface normal (default true)
	 * @param bRandomYaw - Apply random yaw rotation (default true)
	 * @param Seed - Random seed for reproducibility (0 = random)
	 * @param LandscapeNameOrLabel - Optional landscape to constrain placement to
	 * @return Scatter result with instance counts
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static FFoliageScatterResult ScatterFoliageRect(
		const FString& MeshOrFoliageTypePath,
		float WorldMinX,
		float WorldMinY,
		float WorldMaxX,
		float WorldMaxY,
		int32 Count,
		float MinScale = 0.8f,
		float MaxScale = 1.2f,
		bool bAlignToNormal = true,
		bool bRandomYaw = true,
		int32 Seed = 0,
		const FString& LandscapeNameOrLabel = TEXT(""));

	/**
	 * Place individual foliage instances at specific locations.
	 *
	 * @param MeshOrFoliageTypePath - Path to UStaticMesh or UFoliageType asset
	 * @param Locations - Array of world positions
	 * @param MinScale - Minimum random scale (default 1.0)
	 * @param MaxScale - Maximum random scale (default 1.0)
	 * @param bAlignToNormal - Align to surface normal (default true)
	 * @param bRandomYaw - Apply random yaw rotation (default true)
	 * @param bTraceToSurface - Trace downward to find ground (default true)
	 * @return Scatter result with instance counts
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static FFoliageScatterResult AddFoliageInstances(
		const FString& MeshOrFoliageTypePath,
		const TArray<FVector>& Locations,
		float MinScale = 1.0f,
		float MaxScale = 1.0f,
		bool bAlignToNormal = true,
		bool bRandomYaw = true,
		bool bTraceToSurface = true);

	// =================================================================
	// Layer-Aware Placement
	// =================================================================

	/**
	 * Scatter foliage only where a specific landscape paint layer is dominant.
	 * Checks layer weights at each candidate position and only places where
	 * the layer weight exceeds the threshold.
	 *
	 * @param MeshOrFoliageTypePath - Path to UStaticMesh or UFoliageType asset
	 * @param LandscapeNameOrLabel - Name or label of the landscape
	 * @param LayerName - Paint layer name to check (e.g. "Grass")
	 * @param Count - Target number of instances
	 * @param MinScale - Minimum random scale (default 0.8)
	 * @param MaxScale - Maximum random scale (default 1.2)
	 * @param LayerWeightThreshold - Minimum layer weight for placement (0.0-1.0, default 0.5)
	 * @param bAlignToNormal - Align to surface normal (default true)
	 * @param bRandomYaw - Apply random yaw rotation (default true)
	 * @param Seed - Random seed for reproducibility (0 = random)
	 * @return Scatter result with instance counts
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static FFoliageScatterResult ScatterFoliageOnLayer(
		const FString& MeshOrFoliageTypePath,
		const FString& LandscapeNameOrLabel,
		const FString& LayerName,
		int32 Count,
		float MinScale = 0.8f,
		float MaxScale = 1.2f,
		float LayerWeightThreshold = 0.5f,
		bool bAlignToNormal = true,
		bool bRandomYaw = true,
		int32 Seed = 0);

	// =================================================================
	// Removal
	// =================================================================

	/**
	 * Remove all instances of a foliage type in a circular region.
	 *
	 * @param MeshOrFoliageTypePath - Path to UStaticMesh or UFoliageType asset
	 * @param WorldCenterX - Center X of removal region
	 * @param WorldCenterY - Center Y of removal region
	 * @param Radius - Radius of removal region in world units
	 * @return Remove result with instance count
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static FFoliageRemoveResult RemoveFoliageInRadius(
		const FString& MeshOrFoliageTypePath,
		float WorldCenterX,
		float WorldCenterY,
		float Radius);

	/**
	 * Remove ALL instances of a foliage type from the level.
	 *
	 * @param MeshOrFoliageTypePath - Path to UStaticMesh or UFoliageType asset
	 * @return Remove result with instance count
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static FFoliageRemoveResult RemoveAllFoliageOfType(
		const FString& MeshOrFoliageTypePath);

	/**
	 * Remove ALL foliage of ALL types from the level.
	 *
	 * @return Remove result with total instance count
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static FFoliageRemoveResult ClearAllFoliage();

	// =================================================================
	// Query
	// =================================================================

	/**
	 * Get foliage instances of a specific type in a circular region.
	 *
	 * @param MeshOrFoliageTypePath - Path to UStaticMesh or UFoliageType asset
	 * @param WorldCenterX - Center X of query region
	 * @param WorldCenterY - Center Y of query region
	 * @param Radius - Radius of query region in world units
	 * @param MaxResults - Maximum number of instances to return (default 100)
	 * @return Query result with instance data
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage")
	static FFoliageQueryResult GetFoliageInRadius(
		const FString& MeshOrFoliageTypePath,
		float WorldCenterX,
		float WorldCenterY,
		float Radius,
		int32 MaxResults = 100);

	// =================================================================
	// Existence Checks
	// =================================================================

	/**
	 * Check if a foliage type asset exists at the given path.
	 *
	 * @param AssetPath - Path to check (UStaticMesh or UFoliageType)
	 * @return True if the asset exists
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage|Exists")
	static bool FoliageTypeExists(const FString& AssetPath);

	/**
	 * Check if any foliage instances exist in the level for a given mesh/type.
	 *
	 * @param MeshOrFoliageTypePath - Path to UStaticMesh or UFoliageType asset
	 * @return True if instances exist
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Foliage|Exists")
	static bool HasFoliageInstances(const FString& MeshOrFoliageTypePath);

private:
	static class UWorld* GetEditorWorld();
	static class AInstancedFoliageActor* GetOrCreateFoliageActor(UWorld* World);
	static class UFoliageType* FindOrCreateFoliageTypeForMesh(
		const FString& MeshOrFoliageTypePath,
		class AInstancedFoliageActor* IFA);
	static class UFoliageType* FindFoliageTypeInIFA(
		const FString& MeshOrFoliageTypePath,
		class AInstancedFoliageActor* IFA);
	static bool TraceToSurface(
		UWorld* World, float X, float Y,
		FVector& OutLocation, FVector& OutNormal);
	static FFoliageScatterResult ScatterInternal(
		const FString& MeshOrFoliageTypePath,
		const TArray<FVector2D>& CandidatePositions,
		int32 Count,
		float MinScale, float MaxScale,
		bool bAlignToNormal, bool bRandomYaw,
		int32 Seed,
		const FString& LandscapeNameOrLabel,
		const FString& LayerName = TEXT(""),
		float LayerWeightThreshold = 0.0f);
};
