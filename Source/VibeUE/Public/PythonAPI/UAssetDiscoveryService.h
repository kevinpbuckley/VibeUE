// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "AssetRegistry/AssetData.h"
#include "UAssetDiscoveryService.generated.h"

/**
 * Asset import/export and Content Browser service exposed directly to Python.
 *
 * Asset search and general CRUD are provided by the engine's AssetTools toolset.
 * This service owns crash-safe image import, texture export, Content Browser
 * selection, and open-editor checks.
 *
 * Python Usage:
 *   import unreal
 *
 *   # Import an image without pumping the task graph inside an MCP call
 *   path, error = unreal.AssetDiscoveryService.import_asset(
 *       "C:/Images/rocks.jpg", "/Game/UI/Textures", "T_Rocks")
 *
 *   # Inspect the current Content Browser selection
 *   asset_data = unreal.AssetDiscoveryService.get_primary_content_browser_selection()
 *   if asset_data:
 *       print(asset_data.package_name)
 *
 * @note All methods are static.
 */
UCLASS(BlueprintType)
class VIBEUE_API UAssetDiscoveryService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Assets")
	static bool ImportTexture(const FString& SourceFilePath, const FString& DestinationPath);

	/**
	 * Import an image file from disk into the Content Browser as a Texture2D.
	 *
	 * Uses the texture factory's direct binary path (FactoryCreateBinary) rather than
	 * AssetTools::ImportAssets/ImportAssetTasks. The high-level import APIs pump the
	 * game-thread task graph, which asserts (RecursionGuard) when invoked from inside an
	 * MCP tool call (those run inside an AsyncTask on the game thread). This path is safe
	 * to call from execute_python_code and from the manage_asset 'import' action.
	 *
	 * Supported formats: png, jpg, jpeg, bmp, tga, dds, exr, hdr, tiff, tif, psd, pcx.
	 *
	 * @param SourceFilePath    - Absolute path to the image file on disk
	 * @param DestinationFolder - Content Browser folder (e.g. "/Game/UI/Textures")
	 * @param AssetName         - Optional asset name; if empty, derived from the file name
	 * @param OutError          - Receives a human-readable error message on failure
	 * @return The created asset's object path (e.g. "/Game/UI/Textures/T_Foo.T_Foo"), or empty on failure
	 *
	 * Example:
	 *   path, err = unreal.AssetDiscoveryService.import_asset("C:/Images/rocks.jpg", "/Game/UI/Textures", "T_Rocks")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Assets")
	static FString ImportAsset(
		const FString& SourceFilePath,
		const FString& DestinationFolder,
		const FString& AssetName,
		FString& OutError);

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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Assets")
	static bool ExportTexture(const FString& AssetPath, const FString& ExportFilePath);

	// ========== Open Assets & Content Browser ==========

	/**
	 * Get the primary asset selected in the Content Browser (first selection).
	 *
	 * @param OutAsset - The primary selected asset (only valid if function returns true)
	 * @return True if an asset is selected, false if no selection
	 *
	 * Python signature: get_primary_content_browser_selection() -> AssetData or None
	 * (the out-param becomes the return value; do NOT pass an AssetData argument)
	 *
	 * Example:
	 *   asset = unreal.AssetDiscoveryService.get_primary_content_browser_selection()
	 *   if asset:
	 *       print(f"Primary selection: {asset.asset_name}")
	 *   else:
	 *       print("No asset selected")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Assets|Editor")
	static bool GetPrimaryContentBrowserSelection(FAssetData& OutAsset);

	/**
	 * Check if a specific asset is currently open in an editor.
	 *
	 * @param AssetPath - Full path to the asset
	 * @return True if the asset is currently open
	 *
	 * Example:
	 *   if unreal.AssetDiscoveryService.is_asset_open("/Game/Blueprints/BP_Player"):
	 *       print("BP_Player is currently being edited")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Assets|Editor")
	static bool IsAssetOpen(const FString& AssetPath);

private:
	/** Helper: get all assets currently selected in the Content Browser. */
	static TArray<FAssetData> GetContentBrowserSelections();
};
