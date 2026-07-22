// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UFabService.generated.h"

/**
 * Fab asset import service (issue #517). It can discover/import the signed-in Epic account's owned
 * library, or search the public zero-price catalog and directly import eligible glTF/GLB listings
 * (including Quixel Megascans) without adding them to the account library.
 *
 * Thin orchestration over Unreal 5.8's built-in Fab plugin: it reuses the Epic login the editor/launcher
 * already holds (via its own EOS session), calls fab.com's owned-library REST feed, and (for import)
 * drives the engine Fab plugin's downloader. These fab.com endpoints are unofficial/reverse-engineered
 * and may change without notice — see docs/design/fab-service-spec.md and the `fab` skill.
 *
 * Owned flow: AuthStatus() → ListLibrary() → GetAsset() → ImportAsset() → ImportStatus().
 * Free flow: SearchFreeCatalog() → explicit EULA acceptance → ImportFreeAsset() → ImportStatus().
 * No purchasing, add-to-library, or entitlement claiming is implemented.
 */
UCLASS(BlueprintType)
class VIBEUE_API UFabService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Report whether an Epic session is available and the Fab library is reachable. RUN THIS FIRST.
	 * Silently reuses the editor/launcher Epic login; if none is available, returns actionable guidance
	 * (open the Fab window / launch from the Epic Games Launcher) rather than failing hard.
	 * @param TimeoutSeconds How long to wait for the (async) login to resolve before reporting pending. Default 15.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Fab")
	static FString AuthStatus(float TimeoutSeconds = 15.0f);

	/**
	 * List the owned Fab library as a COMPACT projection (id, title, type, source, distribution method,
	 * a `compatible` flag for the current engine, thumbnail). Fetched once and cached in-process; pass
	 * Refresh=true to re-fetch. Filters are applied locally.
	 * @param NameFilter    Case-insensitive substring match on the title. Empty = all.
	 * @param TypeFilter    Match on derived type / distribution method (e.g. "pack","plugin","quixel","model"). Empty = all.
	 * @param EngineVersion Only assets supporting this engine (e.g. "5.8"). Empty = the current engine;
	 *                      "all" (or "any") = no engine filter, results carry a `compatible` flag instead.
	 * @param Limit         Max items to return. @param Offset Items to skip (paging). @param Refresh Re-fetch the library.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Fab")
	static FString ListLibrary(const FString& NameFilter = TEXT(""), const FString& TypeFilter = TEXT(""),
	                           const FString& EngineVersion = TEXT(""), int32 Limit = 200, int32 Offset = 0,
	                           bool Refresh = false);

	/**
	 * Full record for one owned asset: all projectVersions (artifactId + engineVersions + platforms),
	 * distribution method, images, and a compatibility summary for the current engine. Use before import
	 * to choose a version / confirm compatibility.
	 * @param AssetId The assetId from ListLibrary.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Fab")
	static FString GetAsset(const FString& AssetId);

	/** Search Fab's public zero-price catalog without changing the user's Fab library. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Fab")
	static FString SearchFreeCatalog(const FString& Query = TEXT(""), const FString& SellerFilter = TEXT(""),
	                               const FString& FormatFilter = TEXT("gltf"), int32 Limit = 20,
	                               const FString& Cursor = TEXT(""));

	/**
	 * Download + import an owned asset into the project. Idempotent (skips assets already imported).
	 * ASYNC: returns immediately with status "queued"/"downloading" — poll ImportStatus(AssetId).
	 * @param AssetId       The assetId from ListLibrary.
	 * @param EngineVersion Version to import; empty = best match for the current engine.
	 * @param Quality       "" | "Low" | "Medium" | "High" | "Raw" (Quixel/Megascans).
	 * @param Format        "" | "gltf" | "fbx" (3D models).
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Fab")
	static FString ImportAsset(const FString& AssetId, const FString& EngineVersion = TEXT(""),
	                           const FString& Quality = TEXT(""), const FString& Format = TEXT(""));

	/**
	 * Directly import a public zero-price listing without adding it to the Fab library. The listing is
	 * revalidated as free before download. Explicit EULA acceptance is required for every call.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Fab")
	static FString ImportFreeAsset(const FString& ListingId, const FString& Quality = TEXT("High"),
	                              const FString& Format = TEXT("gltf"),
	                              const FString& LicenseSlug = TEXT("personal"), bool AcceptEula = false);

	/** Poll a running import: progress % + phase, or the created /Game/... asset paths, or a failure. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Fab")
	static FString ImportStatus(const FString& AssetId);
};
