// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformHttp.h"

// ---------------------------------------------------------------------------
// Fab REST endpoints — UNOFFICIAL, reverse-engineered from the UE 5.8 Fab plugin
// (Engine/Plugins/Fab) and the community egs-api protocol. These strings are
// load-bearing and Epic can change them without notice — they all live here so a
// break is a one-file fix. Cross-check against the engine Fab plugin's
// FabSettings.cpp (base URL, library path) and egs-api-rs/src/api/fab.rs
// (artifact/manifest path) when maintaining. See docs/design/fab-service-spec.md.
// ---------------------------------------------------------------------------

namespace VibeUE::Fab
{
	// Base URL for the Prod environment. NOTE: the engine Fab plugin uses "https://fab.com", but that
	// host 301-redirects to www.fab.com and the redirect strips the Authorization header, so a headless
	// GET 404s. We hit www.fab.com directly — verified live: fab.com/e/.../ue/library → 404,
	// www.fab.com/e/.../ue/library?count=N → 200 with the owned-library JSON.
	inline const TCHAR* ProdBaseUrl() { return TEXT("https://www.fab.com"); }

	// Owned-library feed. Format: {base}/e/accounts/{EpicAccountId}/ue/library?count={N}[&cursor="{next}"]
	// (verbatim from FabMyFolderIntegration.cpp:69-71). Paged via the response's cursors.next.
	inline FString LibraryUrl(const FString& BaseUrl, const FString& EpicAccountId, int32 Count, const FString& Cursor)
	{
		FString CursorPart;
		if (!Cursor.IsEmpty())
		{
			// The cursor value is wrapped in literal double-quotes, exactly as the engine does.
			CursorPart = FString::Printf(TEXT("&cursor=\"%s\""), *Cursor);
		}
		return FString::Printf(TEXT("%s/e/accounts/%s/ue/library?count=%d%s"), *BaseUrl, *EpicAccountId, Count, *CursorPart);
	}

	// Artifact download manifest. UNVERIFIED against live C++ (the engine mints this in fab.com JS,
	// not C++); path taken from the egs-api community protocol. POST, Bearer auth. Returns the
	// BuildPatch manifest URL + signed distribution-point base URLs for an owned artifact version.
	inline FString ArtifactManifestUrl(const FString& BaseUrl, const FString& ArtifactId)
	{
		return FString::Printf(TEXT("%s/e/artifacts/%s/manifest"), *BaseUrl, *ArtifactId);
	}

	// Public catalog endpoints used by the Fab UE web frontend. Free catalog downloads are available
	// anonymously, but callers must still explicitly accept the applicable EULA before importing.
	inline FString CatalogSearchUrl(const FString& BaseUrl, const FString& Query,
	                                const FString& Seller, const FString& Cursor)
	{
		FString Url = FString::Printf(TEXT("%s/i/listings/search?is_free=1"), *BaseUrl);
		if (!Query.IsEmpty()) Url += TEXT("&q=") + FGenericPlatformHttp::UrlEncode(Query);
		if (!Seller.IsEmpty()) Url += TEXT("&seller=") + FGenericPlatformHttp::UrlEncode(Seller);
		if (!Cursor.IsEmpty()) Url += TEXT("&cursor=") + FGenericPlatformHttp::UrlEncode(Cursor);
		return Url;
	}

	inline FString CatalogListingUrl(const FString& BaseUrl, const FString& ListingId)
	{
		return FString::Printf(TEXT("%s/i/listings/%s"), *BaseUrl, *ListingId);
	}

	inline FString CatalogFormatsUrl(const FString& BaseUrl, const FString& ListingId)
	{
		return FString::Printf(TEXT("%s/i/listings/%s/asset-formats"), *BaseUrl, *ListingId);
	}

	inline FString CatalogDownloadInfoUrl(const FString& BaseUrl, const FString& ListingId,
	                                     const FString& Format, const FString& FileUid,
	                                     const FString& Platform)
	{
		return FString::Printf(TEXT("%s/i/listings/%s/asset-formats/%s/files/%s/download-info?platform=%s"),
			*BaseUrl, *ListingId, *Format, *FileUid, *FGenericPlatformHttp::UrlEncode(Platform));
	}

	// The cooked credentials asset the engine Fab plugin ships (UEosConstants UDataAsset). We read
	// Prod.{ProductId,SandboxId,DeploymentId,ClientCredentialsId,ClientCredentialsSecret,EncryptionKey}
	// off it reflectively so we never depend on the Fab plugin's private header.
	inline const TCHAR* EosCredsAssetPath() { return TEXT("/Fab/Data/FabEos.FabEos"); }
}
