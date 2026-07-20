// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** One engine/version entry from a library item's projectVersions[]. */
struct FFabProjectVersion
{
	FString ArtifactId;
	TArray<FString> EngineVersions;
	TArray<FString> TargetPlatforms;
	TArray<FString> BuildVersions;
};

/** A single owned Fab library asset, parsed from the /ue/library feed. */
struct FFabLibraryAsset
{
	FString AssetId;
	FString AssetNamespace;
	FString Title;
	FString Description;
	FString ListingType;
	FString Seller;
	FString Source;              // e.g. "quixel", "sketchfab", "unreal-engine"
	FString DistributionMethod;  // e.g. "asset_pack", "complete_project", "engine_plugin"
	FString Url;
	TArray<FFabProjectVersion> ProjectVersions;

	struct FImage { FString Url; FString Type; int32 Width = 0; int32 Height = 0; };
	TArray<FImage> Images;

	/** True if any projectVersion advertises support for EngineVersion (e.g. "5.8"). */
	bool SupportsEngine(const FString& EngineVersion) const;

	/** All engine versions this asset advertises across its projectVersions, de-duplicated. */
	TArray<FString> AllEngineVersions() const;

	/** Best-effort asset type used to route import (derived from source/distributionMethod/listingType). */
	FString DeriveAssetType() const;

	/** The artifactId of the projectVersion supporting EngineVersion (or the first, or empty). */
	FString ArtifactIdForEngine(const FString& EngineVersion) const;
};

/**
 * Fetches the signed-in account's owned Fab library synchronously (bounded), paging via cursors.next.
 * Mirrors the engine Fab plugin's request exactly (FabMyFolderIntegration.cpp): GET
 * {base}/e/accounts/{id}/ue/library?count=N, headers accept + Bearer.
 */
class FVibeFabLibrary
{
public:
	/**
	 * @param BaseUrl        e.g. https://fab.com
	 * @param EpicAccountId  stringified EOS account id
	 * @param BearerToken    EOS access token
	 * @param PageSize       'count' per request (engine default 1000)
	 * @param TimeoutSeconds overall budget across all pages
	 * @param OutAssets      parsed library
	 * @param OutHttpCode    last HTTP status (for diagnostics; 200 on success)
	 * @param OutError       set on failure
	 * @return true on success (an empty library is success)
	 */
	static bool Fetch(const FString& BaseUrl, const FString& EpicAccountId, const FString& BearerToken,
	                  int32 PageSize, double TimeoutSeconds,
	                  TArray<FFabLibraryAsset>& OutAssets, int32& OutHttpCode, FString& OutError);
};
