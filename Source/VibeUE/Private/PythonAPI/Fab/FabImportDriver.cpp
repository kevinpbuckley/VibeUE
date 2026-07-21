// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "FabImportDriver.h"
#include "FabManifestClient.h"

#include "FabDownloader.h"   // engine Fab plugin (FAB_API): FFabDownloadRequest / EFabDownloadType / stats

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "FileUtilities/ZipArchiveReader.h"

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

	bool ExtractZipSafely(const FString& ZipFile, const FString& OutputDir,
	                     TArray<FString>& OutFiles, FString& OutError)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* Handle = PlatformFile.OpenRead(*ZipFile);
		if (!Handle)
		{
			OutError = FString::Printf(TEXT("Could not open downloaded archive '%s'."), *ZipFile);
			return false;
		}

		FZipArchiveReader Reader(Handle);
		if (!Reader.IsValid())
		{
			OutError = TEXT("The downloaded file is not a valid ZIP archive.");
			return false;
		}
		IFileManager::Get().MakeDirectory(*OutputDir, true);

		for (const FString& ArchiveName : Reader.GetFileNames())
		{
			FString Relative = ArchiveName;
			Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (Relative.IsEmpty() || Relative.EndsWith(TEXT("/")))
			{
				continue;
			}
			// Reject absolute paths and parent traversal before writing any archive entry.
			if (!FPaths::IsRelative(Relative) || Relative.StartsWith(TEXT("../"))
				|| Relative.Contains(TEXT("/../")) || !FPaths::CollapseRelativeDirectories(Relative, true))
			{
				OutError = FString::Printf(TEXT("Unsafe path in Fab archive: '%s'."), *ArchiveName);
				return false;
			}

			TArray<uint8> Data;
			if (!Reader.TryReadFile(ArchiveName, Data))
			{
				OutError = FString::Printf(TEXT("Could not extract '%s' from the Fab archive."), *ArchiveName);
				return false;
			}
			const FString OutputFile = FPaths::ConvertRelativePathToFull(FPaths::Combine(OutputDir, Relative));
			const FString FullOutputDir = FPaths::ConvertRelativePathToFull(OutputDir);
			if (!FPaths::IsUnderDirectory(OutputFile, FullOutputDir))
			{
				OutError = FString::Printf(TEXT("Archive path escaped the extraction directory: '%s'."), *ArchiveName);
				return false;
			}
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputFile), true);
			if (!FFileHelper::SaveArrayToFile(Data, *OutputFile))
			{
				OutError = FString::Printf(TEXT("Could not write extracted Fab file '%s'."), *OutputFile);
				return false;
			}
			OutFiles.Add(OutputFile);
		}
		return true;
	}

	void ImportDirectSource(const TSharedPtr<FFabImportProgress>& State, const FString& SourceFile)
	{
		check(IsInGameThread());
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = SourceFile;
		Task->DestinationPath = State->DestinationPath;
		Task->bAutomated = true;
		Task->bSave = true;
		Task->bAsync = false;
		Task->bReplaceExisting = false;
		Task->bReplaceExistingSettings = false;

		TArray<UAssetImportTask*> Tasks{Task};
		FAssetToolsModule::GetModule().Get().ImportAssetTasks(Tasks);
		for (const FString& ObjectPath : Task->ImportedObjectPaths)
		{
			State->AssetPaths.AddUnique(FPackageName::ObjectPathToPackageName(ObjectPath));
		}
		if (State->AssetPaths.IsEmpty())
		{
			State->Phase = FFabImportProgress::EPhase::Failed;
			State->Error = FString::Printf(TEXT("Unreal could not import '%s'. Check the Interchange import log."), *SourceFile);
			return;
		}
		State->InstallRoot = State->DestinationPath;
		State->Percent = 100.0f;
		State->Phase = FFabImportProgress::EPhase::Done;
		UE_LOG(LogFabImport, Log, TEXT("Imported public Fab source '%s' into '%s' (%d assets)"),
			*SourceFile, *State->DestinationPath, State->AssetPaths.Num());
	}

	// AssetTools/Interchange may synchronously pump the task graph. Calling it from an AsyncTask
	// game-thread callback re-enters the active task queue and asserts in TaskGraph.cpp. A core ticker
	// callback runs on the game thread from the normal frame loop, outside a task-graph task.
	void ScheduleDirectImport(const TSharedPtr<FFabImportProgress>& State, const FString& SourceFile)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[State, SourceFile](float)
			{
				ImportDirectSource(State, SourceFile);
				return false;
			}));
	}

	void PrepareDirectImport(const TSharedPtr<FFabImportProgress>& State, const FString& DownloadedFile,
	                         const FString& CacheRoot)
	{
		State->Phase = FFabImportProgress::EPhase::Importing;
		const FString Extension = FPaths::GetExtension(DownloadedFile).ToLower();
		if (Extension == TEXT("gltf") || Extension == TEXT("glb"))
		{
			ScheduleDirectImport(State, DownloadedFile);
			return;
		}
		if (Extension != TEXT("zip"))
		{
			State->Phase = FFabImportProgress::EPhase::Failed;
			State->Error = FString::Printf(TEXT("Unsupported free Fab download type '.%s'; expected ZIP, glTF, or GLB."), *Extension);
			return;
		}

		Async(EAsyncExecution::ThreadPool, [State, DownloadedFile, CacheRoot]()
		{
			TArray<FString> Extracted;
			FString Error;
			const FString ExtractRoot = FPaths::Combine(CacheRoot, TEXT("Extracted"));
			if (!ExtractZipSafely(DownloadedFile, ExtractRoot, Extracted, Error))
			{
				State->Phase = FFabImportProgress::EPhase::Failed;
				State->Error = Error;
				return;
			}
			const FString* Source = Extracted.FindByPredicate([](const FString& File)
			{
				return FPaths::GetExtension(File).Equals(TEXT("gltf"), ESearchCase::IgnoreCase);
			});
			if (!Source)
			{
				Source = Extracted.FindByPredicate([](const FString& File)
				{
					return FPaths::GetExtension(File).Equals(TEXT("glb"), ESearchCase::IgnoreCase);
				});
			}
			if (!Source)
			{
				State->Phase = FFabImportProgress::EPhase::Failed;
				State->Error = TEXT("The selected Fab archive contained no glTF or GLB source file.");
				return;
			}
			const FString SourceFile = *Source;
			ScheduleDirectImport(State, SourceFile);
		});
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

bool FVibeFabImport::StartDirect(const FString& AssetId, const FString& AssetName, const FString& DownloadUrl,
	                              const FString& DestinationPath, FString& OutError)
{
	if (TSharedPtr<FFabImportProgress>* Existing = GImports.Find(AssetId))
	{
		if ((*Existing)->Phase != FFabImportProgress::EPhase::Failed)
		{
			return true;
		}
	}
	if (!DownloadUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("Fab returned a non-HTTPS download URL.");
		return false;
	}
	if (!DestinationPath.StartsWith(TEXT("/Game/Fab/Free/")))
	{
		OutError = TEXT("Direct free assets must be imported beneath /Game/Fab/Free/.");
		return false;
	}

	TSharedPtr<FFabImportProgress> State = MakeShared<FFabImportProgress>();
	State->AssetName = AssetName;
	State->DestinationPath = DestinationPath;
	State->Phase = FFabImportProgress::EPhase::Downloading;
	GImports.Add(AssetId, State);

	const FString CacheRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Fab/Free"), AssetId));
	IFileManager::Get().MakeDirectory(*CacheRoot, true);

	TSharedPtr<FFabDownloadRequest> Request = MakeShared<FFabDownloadRequest>(
		AssetId, DownloadUrl, CacheRoot, EFabDownloadType::HTTP);
	State->DownloadKeepAlive = Request;
	TWeakPtr<FFabImportProgress> WeakState = State;
	Request->OnDownloadProgress().AddLambda(
		[WeakState](const FFabDownloadRequest*, const FFabDownloadStats& Stats)
		{
			if (TSharedPtr<FFabImportProgress> Progress = WeakState.Pin())
			{
				Progress->Percent = Stats.PercentComplete;
				Progress->CompletedBytes = Stats.CompletedBytes;
				Progress->TotalBytes = Stats.TotalBytes;
			}
		});
	Request->OnDownloadComplete().AddLambda(
		[WeakState, CacheRoot](const FFabDownloadRequest*, const FFabDownloadStats& Stats)
		{
			TSharedPtr<FFabImportProgress> Progress = WeakState.Pin();
			if (!Progress)
			{
				return;
			}
			Progress->bCached = Stats.bIsCached;
			if (!Stats.bIsSuccess || Stats.DownloadedFiles.IsEmpty())
			{
				Progress->Phase = FFabImportProgress::EPhase::Failed;
				Progress->Error = TEXT("Fab file download failed or returned no file.");
				return;
			}
			PrepareDirectImport(Progress, Stats.DownloadedFiles[0], CacheRoot);
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
