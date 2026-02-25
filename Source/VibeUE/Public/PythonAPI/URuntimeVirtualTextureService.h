// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "URuntimeVirtualTextureService.generated.h"

class URuntimeVirtualTexture;

/**
 * Result of RVT asset creation
 */
USTRUCT(BlueprintType)
struct FRVTCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	FString ErrorMessage;
};

/**
 * Information about an RVT asset
 */
USTRUCT(BlueprintType)
struct FRVTInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	FString AssetPath;

	/** Material type: BaseColor, BaseColor_Normal_Roughness, WorldHeight, etc. */
	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	FString MaterialType;

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	int32 TileCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	int32 TileSize = 0;

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	int32 TileBorderSize = 0;

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	bool bContinuousUpdate = false;

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	bool bSinglePhysicalSpace = false;

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	FString ErrorMessage;
};

/**
 * Result of RVT volume actor creation
 */
USTRUCT(BlueprintType)
struct FRVTVolumeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	bool bSuccess = false;

	/** Actor name of the created volume */
	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	FString VolumeName;

	/** Actor label of the created volume */
	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	FString VolumeLabel;

	UPROPERTY(BlueprintReadWrite, Category = "RVT")
	FString ErrorMessage;
};

/**
 * Service for managing Runtime Virtual Textures (RVT).
 *
 * Runtime Virtual Textures enable efficient rendering of large landscapes by
 * caching material results into virtual texture pages. This service provides
 * tools to create RVT assets, place RVT volumes in levels, and assign RVTs
 * to landscape actors.
 *
 * Python usage:
 *   import unreal
 *
 *   # Create an RVT asset
 *   result = unreal.RuntimeVirtualTextureService.create_runtime_virtual_texture(
 *       "RVT_MyLandscape", "/Game/RVT",
 *       material_type="BaseColor_Normal_Roughness"
 *   )
 *
 *   # Get info about an existing RVT
 *   info = unreal.RuntimeVirtualTextureService.get_runtime_virtual_texture_info(
 *       "/Game/RVT/RVT_MyLandscape"
 *   )
 *
 *   # Create a volume covering a landscape
 *   vol = unreal.RuntimeVirtualTextureService.create_rvt_volume(
 *       "Landscape", "/Game/RVT/RVT_MyLandscape"
 *   )
 *
 *   # Assign RVT to a landscape component
 *   unreal.RuntimeVirtualTextureService.assign_rvt_to_landscape(
 *       "Landscape", "/Game/RVT/RVT_MyLandscape"
 *   )
 */
UCLASS(BlueprintType)
class VIBEUE_API URuntimeVirtualTextureService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Asset Creation
	// =================================================================

	/**
	 * Create a Runtime Virtual Texture asset.
	 * Maps to action="create_runtime_virtual_texture"
	 *
	 * Creates a new URuntimeVirtualTexture asset with the specified configuration.
	 *
	 * MaterialType options:
	 *  - "BaseColor" — Only base color (1 layer)
	 *  - "BaseColor_Normal_Roughness" — Standard PBR (most common for landscapes)
	 *  - "BaseColor_Normal_Specular" — PBR with specular instead of roughness
	 *  - "WorldHeight" — Height field only (used for distance fields)
	 *
	 * @param AssetName - Name for the new RVT asset
	 * @param DirectoryPath - Content directory to create in (e.g., "/Game/RVT")
	 * @param MaterialType - Virtual texture material type (default: "BaseColor_Normal_Roughness")
	 * @param TileCount - Number of tiles (default: 256, controls virtual resolution)
	 * @param TileSize - Size of each tile in pixels (default: 256)
	 * @param TileBorderSize - Border pixels for filtering (default: 4)
	 * @param bContinuousUpdate - If true, update every frame (default: false)
	 * @param bSinglePhysicalSpace - If true, use single physical space (default: false)
	 * @return Result with asset path and any errors
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|RVT")
	static FRVTCreateResult CreateRuntimeVirtualTexture(
		const FString& AssetName,
		const FString& DirectoryPath,
		const FString& MaterialType = TEXT("BaseColor_Normal_Roughness"),
		int32 TileCount = 256,
		int32 TileSize = 256,
		int32 TileBorderSize = 4,
		bool bContinuousUpdate = false,
		bool bSinglePhysicalSpace = false);

	// =================================================================
	// Introspection
	// =================================================================

	/**
	 * Get information about an existing Runtime Virtual Texture asset.
	 * Maps to action="get_runtime_virtual_texture_info"
	 *
	 * @param AssetPath - Full path to the RVT asset
	 * @return Info struct with configuration details
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|RVT")
	static FRVTInfo GetRuntimeVirtualTextureInfo(const FString& AssetPath);

	// =================================================================
	// Level Integration
	// =================================================================

	/**
	 * Create a RuntimeVirtualTextureVolume actor covering a landscape.
	 * Maps to action="create_rvt_volume"
	 *
	 * Creates a volume actor sized to match the landscape bounds.
	 * The volume defines the area where the RVT is active.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the target landscape actor
	 * @param RVTAssetPath - Path to the RVT asset to use
	 * @param VolumeName - Optional name for the volume actor (auto-generated if empty)
	 * @return Result with volume actor info and any errors
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|RVT")
	static FRVTVolumeResult CreateRVTVolume(
		const FString& LandscapeNameOrLabel,
		const FString& RVTAssetPath,
		const FString& VolumeName = TEXT(""));

	/**
	 * Assign an RVT to a landscape component's virtual texture slots.
	 * Maps to action="assign_rvt_to_landscape"
	 *
	 * Sets the RVT on the landscape's RuntimeVirtualTextures array at the given slot.
	 * The landscape will output to this RVT during rendering.
	 *
	 * @param LandscapeNameOrLabel - Name or label of the target landscape actor
	 * @param RVTAssetPath - Path to the RVT asset to assign
	 * @param SlotIndex - Slot index in the landscape's RVT array (default: 0)
	 * @return True if assignment succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|RVT")
	static bool AssignRVTToLandscape(
		const FString& LandscapeNameOrLabel,
		const FString& RVTAssetPath,
		int32 SlotIndex = 0);

private:
	static URuntimeVirtualTexture* LoadRVTAsset(const FString& AssetPath);
};
