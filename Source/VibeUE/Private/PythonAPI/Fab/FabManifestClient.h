// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Resolved download for an owned artifact version. */
struct FFabDownloadInfo
{
	bool bIsBuildPatch = false;         // true = BuildPatchServices manifest; false = direct HTTP file
	FString ManifestOrFileUrl;          // signed BuildPatch manifest URL, or (future) a direct file URL
	TArray<FString> BaseUrls;           // distributionPointBaseUrls (BuildPatch CloudDirectories)
	FString AssetFormat;                // e.g. "asset-format/game-engine/unreal-engine"
	FString BuildVersion;               // e.g. "5.5.0-31272837+++UE5+Dev-Marketplace-Windows"
	FString RawType;                    // downloadInfo[].type as reported ("manifest", ...)

	/** DownloadURL string for FFabDownloadRequest (BuildPatch): "manifestUrl,baseUrl1,baseUrl2,...". */
	FString ToDownloadUrl() const;
};

/**
 * Resolves an owned artifact into a concrete download — the step the engine Fab plugin performs in
 * fab.com JS, not C++. Verified live: POST {base}/e/artifacts/{artifactId}/manifest with body
 * {"namespace":<assetNamespace>,"itemId":<assetId UUID4 hex>,"platform":"Windows"} returns
 * downloadInfo[] with distributionPoints[].manifestUrl (signed) + distributionPointBaseUrls[].
 * Unofficial/reverse-engineered — see docs/design/fab-service-spec.md.
 */
class FVibeFabManifest
{
public:
	/**
	 * @param Platform e.g. "Windows".
	 * @return true on success with OutInfo populated. On a non-BuildPatch asset format, returns false
	 *         with OutError describing the unsupported format (OutInfo.RawType carries the type).
	 */
	static bool Fetch(const FString& BaseUrl, const FString& ArtifactId, const FString& AssetNamespace,
	                  const FString& AssetId, const FString& Platform, const FString& BearerToken,
	                  double TimeoutSeconds, FFabDownloadInfo& OutInfo, int32& OutHttpCode, FString& OutError);
};
