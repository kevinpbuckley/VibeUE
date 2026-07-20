// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

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
	// Base URL for the Prod environment — matches UFabSettings::GetUrlFromEnvironment()
	// (Engine/Plugins/Fab/Source/Fab/Private/FabSettings.cpp): EFabEnvironment::Prod.
	inline const TCHAR* ProdBaseUrl() { return TEXT("https://fab.com"); }

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

	// The cooked credentials asset the engine Fab plugin ships (UEosConstants UDataAsset). We read
	// Prod.{ProductId,SandboxId,DeploymentId,ClientCredentialsId,ClientCredentialsSecret,EncryptionKey}
	// off it reflectively so we never depend on the Fab plugin's private header.
	inline const TCHAR* EosCredsAssetPath() { return TEXT("/Fab/Data/FabEos.FabEos"); }
}
