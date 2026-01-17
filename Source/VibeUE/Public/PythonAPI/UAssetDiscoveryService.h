// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AssetRegistry/AssetData.h"
#include "UAssetDiscoveryService.generated.h"

/**
 * Asset discovery service exposed directly to Python.
 *
 * This service provides asset search and discovery functionality with native
 * Unreal Engine types, eliminating the need for JSON serialization/deserialization.
 *
 * Python Usage:
 *   import unreal
 *
 *   # Search for assets
 *   assets = unreal.AssetDiscoveryService.search_assets("BP_", "Blueprint")
 *   for asset in assets:
 *       print(asset.asset_name, asset.package_path)
 *
 *   # Get assets by type
 *   textures = unreal.AssetDiscoveryService.get_assets_by_type("Texture2D")
 *
 *   # Find specific asset
 *   asset_data = unreal.AssetData()
 *   found = unreal.AssetDiscoveryService.find_asset_by_path("/Game/MyAsset", asset_data)
 *   if found:
 *       print(f"Found: {asset_data.asset_name}")
 *
 * @note All methods are static and thread-safe
 * @note This replaces the JSON-based manage_asset MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UAssetDiscoveryService : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Search for assets by name pattern and optional type filter.
	 *
	 * @param SearchTerm - Pattern to match against asset names (case-insensitive)
	 * @param AssetType - Optional type filter (e.g., "Blueprint", "Texture2D", "StaticMesh")
	 * @return Array of matching assets
	 *
	 * Example:
	 *   assets = unreal.AssetDiscoveryService.search_assets("BP_Player_Test", "Blueprint")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static TArray<FAssetData> SearchAssets(
		const FString& SearchTerm,
		const FString& AssetType = TEXT(""));

	/**
	 * Get all assets of a specific type.
	 *
	 * @param AssetType - The asset class to filter by (e.g., "Blueprint", "Texture2D")
	 * @return Array of assets of the specified type
	 *
	 * Example:
	 *   blueprints = unreal.AssetDiscoveryService.get_assets_by_type("Blueprint")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static TArray<FAssetData> GetAssetsByType(const FString& AssetType);

	/**
	 * Find an asset by its exact path.
	 *
	 * @param AssetPath - Full asset path (e.g., "/Game/Blueprints/BP_Player_Test")
	 * @param OutAsset - The found asset data (only valid if function returns true)
	 * @return True if asset was found, false otherwise
	 *
	 * Example:
	 *   asset = unreal.AssetData()
	 *   if unreal.AssetDiscoveryService.find_asset_by_path("/Game/BP_Player_Test", asset):
	 *       print(f"Found: {asset.asset_name}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static bool FindAssetByPath(const FString& AssetPath, FAssetData& OutAsset);

	/**
	 * Get asset dependencies (hard references).
	 *
	 * @param AssetPath - Path to the asset
	 * @return Array of dependency asset paths
	 *
	 * Example:
	 *   deps = unreal.AssetDiscoveryService.get_asset_dependencies("/Game/BP_Player_Test")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static TArray<FString> GetAssetDependencies(const FString& AssetPath);

	/**
	 * Get assets that reference this asset (hard references).
	 *
	 * @param AssetPath - Path to the asset
	 * @return Array of referencing asset paths
	 *
	 * Example:
	 *   refs = unreal.AssetDiscoveryService.get_asset_referencers("/Game/BP_Player_Test")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static TArray<FString> GetAssetReferencers(const FString& AssetPath);

	/**
	 * List all assets in a specific path (recursive).
	 *
	 * @param Path - Base path to search (e.g., "/Game/Blueprints")
	 * @param AssetType - Optional type filter
	 * @return Array of assets under the specified path
	 *
	 * Example:
	 *   assets = unreal.AssetDiscoveryService.list_assets_in_path("/Game/Blueprints", "Blueprint")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static TArray<FAssetData> ListAssetsInPath(
		const FString& Path,
		const FString& AssetType = TEXT(""));

	// ========== Asset Operations ==========

	/**
	 * Open an asset in its appropriate editor.
	 *
	 * @param AssetPath - Full path to the asset (e.g., "/Game/Blueprints/BP_Player")
	 * @return True if asset was opened successfully
	 *
	 * Example:
	 *   unreal.AssetDiscoveryService.open_asset("/Game/Blueprints/BP_Player")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static bool OpenAsset(const FString& AssetPath);

	/**
	 * Delete an asset from the project.
	 *
	 * @param AssetPath - Full path to the asset to delete
	 * @return True if asset was deleted successfully
	 *
	 * Example:
	 *   unreal.AssetDiscoveryService.delete_asset("/Game/Blueprints/BP_OldActor")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static bool DeleteAsset(const FString& AssetPath);

	/**
	 * Duplicate an asset to a new location.
	 *
	 * @param SourcePath - Path to the source asset
	 * @param DestinationPath - Path for the duplicated asset
	 * @return True if duplication was successful
	 *
	 * Example:
	 *   unreal.AssetDiscoveryService.duplicate_asset("/Game/BP_Player", "/Game/BP_Player_Copy")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static bool DuplicateAsset(const FString& SourcePath, const FString& DestinationPath);

	/**
	 * Save a specific asset.
	 *
	 * @param AssetPath - Full path to the asset to save
	 * @return True if asset was saved successfully
	 *
	 * Example:
	 *   unreal.AssetDiscoveryService.save_asset("/Game/Blueprints/BP_Player")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static bool SaveAsset(const FString& AssetPath);

	/**
	 * Save all dirty (modified) assets in the project.
	 *
	 * @return Number of assets saved
	 *
	 * Example:
	 *   count = unreal.AssetDiscoveryService.save_all_assets()
	 *   print(f"Saved {count} assets")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static int32 SaveAllAssets();

	// ========== Texture Operations ==========

	/**
	 * Import a texture from the file system into the project.
	 *
	 * @param SourceFilePath - Absolute path to the texture file (PNG, JPG, TGA, etc.)
	 * @param DestinationPath - Asset path where texture will be created (e.g., "/Game/Textures/MyTexture")
	 * @return True if import was successful
	 *
	 * Example:
	 *   unreal.AssetDiscoveryService.import_texture("C:/Images/logo.png", "/Game/Textures/Logo")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static bool ImportTexture(const FString& SourceFilePath, const FString& DestinationPath);

	/**
	 * Export a texture to the file system for external analysis.
	 *
	 * @param AssetPath - Path to the texture asset
	 * @param ExportFilePath - Absolute path where the texture will be exported
	 * @return True if export was successful
	 *
	 * Example:
	 *   unreal.AssetDiscoveryService.export_texture("/Game/Textures/MyTexture", "C:/Exports/texture.png")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets")
	static bool ExportTexture(const FString& AssetPath, const FString& ExportFilePath);

	// ========== Existence Checks ==========

	/**
	 * Check if any asset exists at the given path. Generic fallback for all asset types.
	 *
	 * @param AssetPath - Full path to the asset
	 * @return True if asset exists
	 *
	 * Example:
	 *   if not unreal.AssetDiscoveryService.asset_exists("/Game/Materials/M_Base"):
	 *       # Create the material
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Assets|Exists")
	static bool AssetExists(const FString& AssetPath);
};
