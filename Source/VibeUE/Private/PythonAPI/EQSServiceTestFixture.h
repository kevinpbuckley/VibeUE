// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

/**
 * Fixture-asset hygiene for the EQS test suite.
 *
 * Deliberately duplicated from AIServiceTestFixture.h rather than shared: that header is consumed
 * by the Behavior Tree and Blackboard suites, which sit in an open upstream PR, and touching it
 * would dirty that branch. The semantics are identical; a fix in one belongs in the other.
 *
 * NOTE: Content/Developers is NOT reliably gitignored — it is not in this repo, and its .uasset
 * files are LFS-tracked here — so entry+exit resets are the only thing standing between a
 * crashed run and test fixtures landing in version control. Host projects should either ignore
 * Content/Developers or review `git status` after a run that failed.
 */
namespace VibeEQSTest
{
	/**
	 * Forcibly remove every trace of AssetPath: the in-memory UObject/UPackage (possible whenever
	 * this process has already run the suite once without restarting the editor) AND the .uasset
	 * file on disk (left by a *previous process's* run — Content/Developers is gitignored, so
	 * nothing else ever cleans it up). Without this, a second run collides with the first run's
	 * leftovers and fails for reasons unrelated to what is being tested.
	 *
	 * Deliberately does not go through UEditorAssetLibrary::DeleteAsset: it returns true having
	 * deleted nothing whenever something still references the asset, which mid-suite is the normal
	 * case. The file is deleted directly, and the only thing trusted as proof is asking the
	 * filesystem again afterwards — not this function's return value, not an in-memory or
	 * asset-registry re-query, and emphatically not FPackageName::DoesPackageExist, whose index is
	 * built at process start and still reports a since-deleted file as present.
	 */
	inline void ResetFixtureAsset(const FString& AssetPath)
	{
		// 1) Purge a same-process leftover: detach any existing object(s) from the package name
		//    (renaming into the transient package under a unique name so nothing can collide),
		//    strip the flags keeping them alive, then force a GC pass so the name is free to reuse
		//    and nothing stale is reachable by a later LoadObject/FindObject in this run.
		if (UPackage* ExistingPackage = FindPackage(nullptr, *AssetPath))
		{
			TArray<UObject*> ObjectsInPackage;
			GetObjectsWithPackage(ExistingPackage, ObjectsInPackage, EGetObjectsFlags::IncludeNestedObjects);
			for (UObject* Obj : ObjectsInPackage)
			{
				if (!Obj)
				{
					continue;
				}
				Obj->ClearFlags(RF_Standalone | RF_Public);
				const FString ScratchName = FString::Printf(TEXT("STALE_%s_%s"),
					*Obj->GetName(), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
				Obj->Rename(*ScratchName, GetTransientPackage(),
					REN_DontCreateRedirectors | REN_NonTransactional | REN_AllowPackageLinkerMismatch);
				Obj->MarkAsGarbage();
			}
			ExistingPackage->ClearFlags(RF_Standalone | RF_Public);
			ExistingPackage->MarkAsGarbage();
			CollectGarbage(RF_NoFlags);
		}

		// 2) Delete the .uasset file left by a previous process's run, if any.
		FString Filename;
		if (FPackageName::TryConvertLongPackageNameToFilename(
			AssetPath, Filename, FPackageName::GetAssetPackageExtension()))
		{
			if (IFileManager::Get().FileExists(*Filename))
			{
				IFileManager::Get().Delete(*Filename, /*RequireExists*/ false, /*EvenReadOnly*/ true);
			}
		}

		// 3) Verify against the filesystem — the only acceptable proof — not an API return value.
		if (!Filename.IsEmpty())
		{
			ensureAlwaysMsgf(!IFileManager::Get().FileExists(*Filename),
				TEXT("ResetFixtureAsset: %s still exists on disk after delete"), *Filename);
		}
	}

	/** Absolute .uasset filename for a package path, or empty if the path is not convertible. */
	inline FString FixtureFilename(const FString& AssetPath)
	{
		FString Filename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(
			AssetPath, Filename, FPackageName::GetAssetPackageExtension()))
		{
			return FString();
		}
		return FPaths::ConvertRelativePathToFull(Filename);
	}
}

/**
 * Resets AssetPath's fixture on construction, so the test starts from a genuinely clean slate
 * regardless of what a previous run (this process or a prior one) left behind, and again on
 * destruction, so a normal, non-crashing run leaves nothing behind for the next one.
 *
 * Entry-reset is the half that actually buys robustness: it runs even when the previous test that
 * used this path crashed partway through and never reached its own exit-reset.
 */
struct FEQSScopedFixtureReset
{
	FString AssetPath;

	explicit FEQSScopedFixtureReset(FString InAssetPath) : AssetPath(MoveTemp(InAssetPath))
	{
		VibeEQSTest::ResetFixtureAsset(AssetPath);
	}

	~FEQSScopedFixtureReset()
	{
		VibeEQSTest::ResetFixtureAsset(AssetPath);
	}

	/** Does the .uasset actually exist on disk? Asked of the filesystem, never of the package index. */
	static bool FixtureFileExists(const FString& AssetPath)
	{
		const FString Filename = VibeEQSTest::FixtureFilename(AssetPath);
		return !Filename.IsEmpty() && IFileManager::Get().FileExists(*Filename);
	}

	/** Size in bytes of the .uasset on disk, or -1 if it does not exist. */
	static int64 FixtureFileSize(const FString& AssetPath)
	{
		const FString Filename = VibeEQSTest::FixtureFilename(AssetPath);
		return Filename.IsEmpty() ? -1 : IFileManager::Get().FileSize(*Filename);
	}
};

#endif // WITH_AUTOMATION_TESTS
