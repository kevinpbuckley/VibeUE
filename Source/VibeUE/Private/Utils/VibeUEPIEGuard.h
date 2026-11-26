// Copyright Buckley Builds LLC 2026 All Rights Reserved.
//
// One refusal for every VibeUE service that writes assets, while a Play-In-Editor session runs (#705).
//
// During PIE, UEditorAssetLibrary::LoadAsset returns null (its CheckIfInEditorAndPIE guard). A write path that loads
// through it reports "asset not found", and a script that believes it creates the asset again over the existing one
// with default settings (twelve textures re-imported that way in one week of agent work), or fails later on None.
// So an asset-writing service function starts with VIBEUE_REFUSE_ASSET_WRITE_DURING_PIE("Service.function"): during
// PIE it raises one script error, the same text everywhere, and returns at once; outside PIE it does nothing. Reads
// load through LoadAssetAnyState, which resolves in any editor state (the #671 fix), so they keep working during PIE.
//
// Module-private (Private/, no VIBEUE_API).

#pragma once

#include "CoreMinimal.h"

namespace VibeUE
{
	/** True while a Play-In-Editor or Simulate session runs. */
	bool IsPlaySessionRunning();

	/** "PIE_ACTIVE: <Operation> refused: ... Stop PIE first ..." - the one text every refusal uses (and vibeue.require_no_pie). */
	FString MakePIEWriteRefusal(const FString& Operation);

	/**
	 * During PIE: raise the refusal as a script error (a Python exception, an MCP error result) and return true, so the
	 * caller returns at once. Outside PIE: false, nothing happens. Use the macros below.
	 */
	bool RefuseAssetWriteDuringPIE(const TCHAR* Operation);

	/**
	 * Load an asset in any editor state, PIE included: resolves an already-loaded object first, then loads it.
	 * Accepts "/Game/A/B" and "/Game/A/B.B". UEditorAssetLibrary::LoadAsset returns null during PIE (#671).
	 */
	UObject* LoadAssetAnyState(const FString& AssetPath);
}

/** First statement of an asset-writing service function that returns a value: during PIE it refuses and returns {}. */
#define VIBEUE_REFUSE_ASSET_WRITE_DURING_PIE(Operation) \
	if (::VibeUE::RefuseAssetWriteDuringPIE(TEXT(Operation))) { return {}; }

/** The same for a function that returns void. */
#define VIBEUE_REFUSE_ASSET_WRITE_DURING_PIE_VOID(Operation) \
	if (::VibeUE::RefuseAssetWriteDuringPIE(TEXT(Operation))) { return; }
