// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

class UClass;

/**
 * Blueprint short-name class resolution internals for UBlueprintService. Private to the module;
 * exposed so the automation tests can drive the ambiguity policy with synthetic candidates (two
 * same-named Blueprints created in memory in one test crashed the headless host, 2026-10-23).
 */
namespace VibeBlueprintClassResolve
{
	/** One Blueprint package that owns the requested short name. */
	struct FCandidate
	{
		FName PackageName;
		FString ObjectPath;
		UClass* Class = nullptr; // set when the Blueprint is loaded and compiled
		FAssetData Asset;        // otherwise, the registry entry to load
	};

	/**
	 * The short-name ambiguity policy, applied to candidates already de-duplicated by package:
	 *   - none: nullptr;
	 *   - one: its loaded generated class, else the registry asset is loaded and its GeneratedClass
	 *     returned;
	 *   - two or more: REFUSED. Logs a Warning
	 *     "ResolveClassByName: short name '<Short>' is ambiguous, <N> Blueprints match: <paths>. ..."
	 *     with every candidate's object path, sorted, and returns nullptr. There is no implicit
	 *     preference (not even /Game over /Engine or plugin content): pass a full path instead.
	 */
	UClass* ChooseCandidate(const FString& ShortName, const TArray<FCandidate>& Candidates);
}
