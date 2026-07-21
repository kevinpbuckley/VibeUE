// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FFabDownloadInfo;

/** Live state of one asset's import, polled by FabService::ImportStatus. */
struct FFabImportProgress
{
	enum class EPhase : uint8 { Downloading, Importing, Done, Failed };

	EPhase Phase = EPhase::Downloading;
	float Percent = 0.0f;
	uint64 CompletedBytes = 0;
	uint64 TotalBytes = 0;
	bool bCached = false;
	FString Error;
	FString AssetName;
	TArray<FString> AssetPaths;    // /Game/... packages created by the import
	FString InstallRoot;           // /Game/<top folder> if derivable
	FString DestinationPath;       // Direct-file imports: requested /Game destination

	// BuildPatch doesn't report installed files, so we diff the set of asset files under the install
	// root around the download to discover what landed (robust to pre-existing/empty folders).
	bool bIsPluginImport = false;
	FString ScanRootDir;                 // <Project>/Content (or /Plugins) on disk
	TSet<FString> PreExistingFiles;      // .uasset/.umap present before the download

	// Keeps the underlying FFabDownloadRequest alive for the life of the import (its delegates
	// reference this progress via a weak ptr, so there is no ownership cycle).
	TSharedPtr<void> DownloadKeepAlive;
};

/**
 * Route R (reimplement-in-VibeUE) import driver: reuses the engine Fab plugin's FAB_API BuildPatch
 * download machinery (FFabDownloadRequest), then reimplements the post-download step (asset-registry
 * scan + /Game path reporting) instead of the engine's Private import workflows. Async — Start() kicks
 * the download and returns; poll via Get(). Supports BuildPatch pack/plugin assets plus public free
 * glTF/GLB ZIP downloads (including Quixel/Megascans) through automated Interchange import.
 */
class FVibeFabImport
{
public:
	/**
	 * Begin importing an owned asset. Idempotent: if a completed import for AssetId exists, returns it.
	 * @param bIsPlugin  route as a UE plugin (installs into the project's Plugins/) vs a content pack.
	 */
	static bool Start(const FString& AssetId, const FString& AssetName, bool bIsPlugin,
	                  const FFabDownloadInfo& Info, FString& OutError);

	/** Begin a direct HTTP download followed by automated glTF/GLB import into DestinationPath. */
	static bool StartDirect(const FString& AssetId, const FString& AssetName, const FString& DownloadUrl,
	                        const FString& DestinationPath, FString& OutError);

	/** Current progress for AssetId, or null if no import has been started this session. */
	static TSharedPtr<FFabImportProgress> Get(const FString& AssetId);
};
