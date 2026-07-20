// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UFabService.generated.h"

/**
 * Fab library import service (issue #517) — discover the signed-in Epic account's OWNED Fab library
 * (free + already-purchased, including migrated UE Marketplace items) and import chosen assets into the
 * project. NO purchasing: this only lists and imports what the account already owns.
 *
 * Thin orchestration over Unreal 5.8's built-in Fab plugin: it reuses the Epic login the editor/launcher
 * already holds (via its own EOS session), calls fab.com's owned-library REST feed, and (for import)
 * drives the engine Fab plugin's downloader. These fab.com endpoints are unofficial/reverse-engineered
 * and act only on the user's own account — see docs/design/fab-service-spec.md and the `fab` skill.
 *
 * Recommended flow: AuthStatus() FIRST (confirms an Epic session) → ListLibrary() → GetAsset() to inspect
 * a version → ImportAsset() then poll ImportStatus(). Methods return a JSON string ({"success": true,...}
 * / {"success": false, "error_code", "error"}).
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
	 * @param EngineVersion Only assets supporting this engine (e.g. "5.8"). Empty = the current engine.
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

	/** Poll a running import: progress % + phase, or the created /Game/... asset paths, or a failure. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Fab")
	static FString ImportStatus(const FString& AssetId);
};
