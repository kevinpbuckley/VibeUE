// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "AssetRegistry/AssetData.h"
#include "UAssetDiscoveryService.generated.h"

/**
 * Result of DeleteAssetUnattended.
 *
 * The function used to return bool with OutReferencers/OutError out-params, and Python maps a
 * false bool return to None, which dropped the referencers AND the reason on every refusal — the
 * exact information the caller needs to recover. A struct return always survives to Python.
 */
USTRUCT(BlueprintType)
struct FUnattendedDeleteResult
{
	GENERATED_BODY()

	/** True only when the asset is gone. */
	UPROPERTY(BlueprintReadWrite, Category = "Assets")
	bool bSuccess = false;

	/** Package paths / object names that referenced the asset (filled on refusal AND on a forced delete). */
	UPROPERTY(BlueprintReadWrite, Category = "Assets")
	TArray<FString> Referencers;

	/** Human-readable reason when the delete did not happen; empty on success. */
	UPROPERTY(BlueprintReadWrite, Category = "Assets")
	FString ErrorMessage;
};

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
	 * Reimport an existing asset through the same handler used by Content Browser Reimport.
	 *
	 * Supports both Interchange and legacy factory imports. When NewSourcePath is supplied,
	 * the registered reimport handler is asked to retarget the asset before reimporting it.
	 * The operation is automated and never opens a missing-file picker or notification.
	 *
	 * @param AssetPath         - Content path or object path of the asset to reimport
	 * @param NewSourcePath     - Optional replacement source file; empty uses the stored source
	 * @param OutSourceFileUsed - Receives the resolved source file selected for reimport
	 * @param OutError          - Receives a human-readable error message on failure
	 * @return True when Unreal's registered reimport handler completed successfully
	 *
	 * Python usage (Unreal maps a false bool plus out parameters to None):
	 *   result = unreal.AssetDiscoveryService.reimport_asset(
	 *       "/Game/Characters/SKM_Player", "D:/Source/SKM_Player.fbx")
	 *   if result is not None:
	 *       source_file, error = result
	 */
	/**
	 * Delete an asset with NO dialog, for unattended agent sessions. The engine's
	 * EditorAssetLibrary.delete_asset pops a modal "asset is referenced" dialog when anything
	 * points at the asset, and an MCP-driven editor cannot answer it: the game thread stalls
	 * until someone force-kills the process. This function never asks. With
	 * bForceEvenIfReferenced it force-deletes and nulls every reference (the same thing the
	 * dialog's Force Delete button does); without it, a referenced asset is refused and the
	 * referencers are returned so the caller can decide.
	 *
	 * IMPLEMENTATION: this replicates the front half of ObjectTools::ForceDeleteObjects
	 * (ObjectTools.cpp:3621-4063) to reach the same refusal DECISION without its modal — clear the
	 * Blueprint action database, OnAssetsCanDelete gate, ForceReplaceReferences(nullptr, ...) to null
	 * every reflected reference in memory, CollectGarbage, then GatherObjectReferencersForDeletion with
	 * default flags (the reachability-filtered check DeleteSingleObject runs at :3500). Where the engine
	 * would pop the modal "is in use" dialog it cannot answer, this refuses and returns the referencer
	 * names instead. That set is exactly what the dialog would have listed: objects still reachable from a
	 * GC root after the replace, i.e. NATIVE references force-replace cannot null (the canonical case is
	 * the UGCObjectReferencer a Python module-level global appears as). A transient helper that holds the
	 * asset natively but is not itself rooted (e.g. an AnimSequencerController) drops out after the GC and
	 * does NOT block, exactly as the engine's own delete succeeds on it.
	 *
	 * On success the ACTUAL deletion is handed to the real ObjectTools::ForceDeleteObjects(bShowConfirmation
	 * =false), so child-Blueprint reparenting, child-redirector/generated-class removal and
	 * UUserDefinedStruct reinstancing keep full engine fidelity. Its own internal "is in use" check cannot
	 * reach a dialog (it re-runs the replace before checking, and we already proved nothing rooted refers
	 * to the asset); the only modal it could still raise is the read-only-package prompt, and only for an
	 * asset whose .uasset is read-only on disk with source control disabled.
	 *
	 * SIDE EFFECT with bForceEvenIfReferenced: the reflected references are nulled (ForceReplaceReferences)
	 * BEFORE the final reachability check, so if that check then refuses, other in-memory objects are left
	 * with their references to this asset already nulled — the same state the engine produces before its
	 * dialog, i.e. what pressing Cancel on that dialog would have left behind. The non-force path is
	 * unchanged: when an on-disk asset references the target it is refused up front (before any replace),
	 * so a non-force call never nulls references inside other on-disk assets.
	 *
	 * Even with bForceEvenIfReferenced, the delete is REFUSED (never prompts) when the object is still
	 * reachable from a GC root after the replace — a native GCObject root or a Python module-level global
	 * that created or loaded it. Those are returned in Referencers with an ErrorMessage telling the caller
	 * to release the globals (del them, then unreal.SystemLibrary.collect_garbage()) and retry.
	 *
	 * @param AssetPath              - Package path of the asset (/Game/Folder/Asset)
	 * @param bForceEvenIfReferenced - True: delete anyway and clear references; false: refuse if referenced
	 * @return FUnattendedDeleteResult: bSuccess, Referencers (filled on refusal AND on force), ErrorMessage
	 *
	 * Python usage (the result struct always comes back, so the reason survives a refusal):
	 *   result = unreal.AssetDiscoveryService.delete_asset_unattended("/Game/Anim/AS_Temp", True)
	 *   if not result.b_success:
	 *       print(result.error_message, result.referencers)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable, CPP_Default_bForceEvenIfReferenced = "false"), Category = "VibeUE|Assets")
	static FUnattendedDeleteResult DeleteAssetUnattended(
		const FString& AssetPath,
		bool bForceEvenIfReferenced);

	UFUNCTION(BlueprintCallable, meta = (AICallable, CPP_Default_NewSourcePath = ""), Category = "VibeUE|Assets")
	static bool ReimportAsset(
		const FString& AssetPath,
		const FString& NewSourcePath,
		FString& OutSourceFileUsed,
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
