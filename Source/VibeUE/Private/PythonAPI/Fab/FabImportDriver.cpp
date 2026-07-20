// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "FabImportDriver.h"
#include "FabManifestClient.h"

#include "FabDownloader.h"   // engine Fab plugin (FAB_API): FFabDownloadRequest / EFabDownloadType / stats

#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogFabImport, Log, All);

namespace
{
	// One import per asset, for the editor session.
	TMap<FString, TSharedPtr<FFabImportProgress>> GImports;

	// Convert an installed file path to a /Game package path, or empty if it isn't a content asset.
	FString ToGamePackage(const FString& AbsFile)
	{
		const FString Ext = FPaths::GetExtension(AbsFile).ToLower();
		if (Ext != TEXT("uasset") && Ext != TEXT("umap"))
		{
			return FString();
		}
		FString Package;
		if (FPackageName::TryConvertFilenameToLongPackageName(AbsFile, Package) && Package.StartsWith(TEXT("/Game")))
		{
			return Package;
		}
		// Fallback: split on /Content/ and rebuild a /Game path.
		int32 Idx = AbsFile.Find(TEXT("/Content/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Idx == INDEX_NONE)
		{
			return FString();
		}
		FString Rel = AbsFile.Mid(Idx + 9);           // after "/Content/"
		Rel = FPaths::GetBaseFilename(Rel, /*bRemovePath*/ false);
		return TEXT("/Game/") + Rel;
	}

	// All .uasset/.umap files under a directory (absolute paths).
	TArray<FString> AllAssetFiles(const FString& Root)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.uasset"), true, false);
		IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.umap"), true, false, /*bClearFileNames*/ false);
		return Files;
	}

	// BuildPatch does not report the files it installs, so we diff the set of asset files under the
	// install root (captured before the download) to find exactly what landed — robust to a
	// pre-existing or empty target folder — then scan those files into the asset registry.
	void FinishImport(const TSharedPtr<FFabImportProgress>& State)
	{
		const TArray<FString> NowFiles = AllAssetFiles(State->ScanRootDir);
		TArray<FString> NewFiles;
		for (const FString& F : NowFiles)
		{
			if (!State->PreExistingFiles.Contains(F))
			{
				NewFiles.Add(F);
			}
		}

		for (const FString& File : NewFiles)
		{
			const FString Pkg = ToGamePackage(File);
			if (!Pkg.IsEmpty())
			{
				State->AssetPaths.AddUnique(Pkg);
			}
		}

		if (NewFiles.Num() > 0)
		{
			IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			AR.ScanFilesSynchronous(NewFiles, /*bForceRescan*/ true);
		}

		// Install root: the shallowest /Game/<top folder> among the new assets.
		for (const FString& Pkg : State->AssetPaths)
		{
			FString Rest = Pkg.RightChop(6);   // after "/Game/"
			int32 Slash = INDEX_NONE;
			if (Rest.FindChar(TEXT('/'), Slash))
			{
				State->InstallRoot = TEXT("/Game/") + Rest.Left(Slash);
				break;
			}
		}

		State->Phase = FFabImportProgress::EPhase::Done;
		UE_LOG(LogFabImport, Log, TEXT("Fab import complete: %d asset(s) (of %d new file(s)), root '%s'"),
			State->AssetPaths.Num(), NewFiles.Num(), *State->InstallRoot);
	}
}

bool FVibeFabImport::Start(const FString& AssetId, const FString& AssetName, bool bIsPlugin,
                           const FFabDownloadInfo& Info, FString& OutError)
{
	if (TSharedPtr<FFabImportProgress>* Existing = GImports.Find(AssetId))
	{
		// Idempotent: a completed or in-flight import is returned as-is.
		if ((*Existing)->Phase != FFabImportProgress::EPhase::Failed)
		{
			return true;
		}
	}

	if (!Info.bIsBuildPatch)
	{
		OutError = TEXT("Only BuildPatch (pack/plugin) assets are supported so far; this asset uses a different download format.");
		return false;
	}

	TSharedPtr<FFabImportProgress> State = MakeShared<FFabImportProgress>();
	State->AssetName = AssetName;
	State->Phase = FFabImportProgress::EPhase::Downloading;
	State->bIsPluginImport = bIsPlugin;
	// Diff root: where the install lands (packs -> project Content; plugins -> project Plugins).
	State->ScanRootDir = bIsPlugin
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir())
		: FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	{
		const TArray<FString> Existing = AllAssetFiles(State->ScanRootDir);
		State->PreExistingFiles.Append(Existing);
	}
	GImports.Add(AssetId, State);

	// Packs install non-destructively into the project dir (content lands under /Game); plugins into Plugins/.
	const FString Location = bIsPlugin
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir())
		: FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const EFabDownloadType Type = bIsPlugin ? EFabDownloadType::BuildPatchInstallRequest : EFabDownloadType::BuildPatchRequest;

	TSharedPtr<FFabDownloadRequest> Request = MakeShared<FFabDownloadRequest>(AssetId, Info.ToDownloadUrl(), Location, Type);
	State->DownloadKeepAlive = Request;

	TWeakPtr<FFabImportProgress> WeakState = State;

	Request->OnDownloadProgress().AddLambda(
		[WeakState](const FFabDownloadRequest*, const FFabDownloadStats& S)
		{
			if (TSharedPtr<FFabImportProgress> P = WeakState.Pin())
			{
				P->Percent = S.PercentComplete;
				P->CompletedBytes = S.CompletedBytes;
				P->TotalBytes = S.TotalBytes;
			}
		});

	Request->OnDownloadComplete().AddLambda(
		[WeakState](const FFabDownloadRequest*, const FFabDownloadStats& S)
		{
			TSharedPtr<FFabImportProgress> P = WeakState.Pin();
			if (!P)
			{
				return;
			}
			P->bCached = S.bIsCached;
			if (!S.bIsSuccess)
			{
				P->Phase = FFabImportProgress::EPhase::Failed;
				P->Error = TEXT("Download/install failed.");
				return;
			}
			// Post-install (folder diff + asset registry scan) must run on the game thread.
			if (IsInGameThread())
			{
				FinishImport(P);
			}
			else
			{
				AsyncTask(ENamedThreads::GameThread, [P]() { FinishImport(P); });
			}
		});

	Request->ExecuteRequest();
	return true;
}

TSharedPtr<FFabImportProgress> FVibeFabImport::Get(const FString& AssetId)
{
	if (TSharedPtr<FFabImportProgress>* Found = GImports.Find(AssetId))
	{
		return *Found;
	}
	return nullptr;
}
