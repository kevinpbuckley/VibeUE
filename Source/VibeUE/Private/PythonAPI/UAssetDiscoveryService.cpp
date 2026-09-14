// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UAssetDiscoveryService.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformFileManager.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Factories/TextureFactory.h"
#include "EditorReimportHandler.h"
#include "UObject/Package.h"
#include "ObjectTools.h"
#include "UObject/ReferencerFinder.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "BlueprintActionDatabase.h"   // A13 false-refusal fix: clear transient node spawners like the engine's own delete path
#include "BlueprintAssetHandler.h"     // A13: engine fallback for a non-Blueprint asset that still owns a UBlueprint
#include "Engine/Blueprint.h"          // A13: complete UBlueprint type for the UBlueprint* -> UObject* base conversion above
#include "UObject/GCObject.h"          // A13: UGCObjectReferencer — the native GC root a Python global appears as (refusal signal)

// ========== Texture Operations ==========

bool UAssetDiscoveryService::ImportTexture(const FString& SourceFilePath, const FString& DestinationPath)
{
	// Split the destination asset path into folder + name and delegate to the safe importer.
	FString PackagePath, AssetName;
	if (!DestinationPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd) || PackagePath.IsEmpty())
	{
		PackagePath = TEXT("/Game");
		AssetName = DestinationPath;
	}

	FString Error;
	const FString Result = ImportAsset(SourceFilePath, PackagePath, AssetName, Error);
	if (Result.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::ImportTexture: %s"), *Error);
		return false;
	}
	return true;
}

FString UAssetDiscoveryService::ImportAsset(
	const FString& SourceFilePath,
	const FString& DestinationFolder,
	const FString& AssetName,
	FString& OutError)
{
	OutError.Empty();

	if (SourceFilePath.IsEmpty() || DestinationFolder.IsEmpty())
	{
		OutError = TEXT("SourceFilePath and DestinationFolder are both required");
		return FString();
	}

	if (!FPaths::FileExists(SourceFilePath))
	{
		OutError = FString::Printf(TEXT("Source file does not exist: %s"), *SourceFilePath);
		return FString();
	}

	// Resolve the asset name (derive from the file name when not provided) and sanitize it.
	FString FinalName = AssetName.IsEmpty() ? FPaths::GetBaseFilename(SourceFilePath) : AssetName;
	{
		FString Sanitized;
		for (TCHAR Ch : FinalName)
		{
			Sanitized.AppendChar((FChar::IsAlnum(Ch) || Ch == TEXT('_')) ? Ch : TEXT('_'));
		}
		FinalName = Sanitized;
	}
	if (FinalName.IsEmpty())
	{
		OutError = TEXT("Could not derive a valid asset name");
		return FString();
	}

	// Normalize the destination folder into a content path.
	FString Folder = DestinationFolder;
	Folder.RemoveFromEnd(TEXT("/"));
	if (!Folder.StartsWith(TEXT("/")))
	{
		OutError = FString::Printf(TEXT("DestinationFolder must be a content path like /Game/...: '%s'"), *DestinationFolder);
		return FString();
	}

	// Only image formats are handled by this fast factory path.
	const FString Ext = FPaths::GetExtension(SourceFilePath).ToLower();
	static const TSet<FString> ImageExts = {
		TEXT("png"), TEXT("jpg"), TEXT("jpeg"), TEXT("bmp"), TEXT("tga"),
		TEXT("dds"), TEXT("exr"), TEXT("hdr"), TEXT("tiff"), TEXT("tif"),
		TEXT("psd"), TEXT("pcx")
	};
	if (!ImageExts.Contains(Ext))
	{
		OutError = FString::Printf(
			TEXT("Unsupported file type '.%s'. Supported image formats: png, jpg, jpeg, bmp, tga, dds, exr, hdr, tiff, tif, psd, pcx."),
			*Ext);
		return FString();
	}

	// Read the file into memory and feed it straight to the texture factory. We deliberately
	// avoid IAssetTools::ImportAssets / ImportAssetTasks: those pump the game-thread task graph,
	// which trips a RecursionGuard assertion when called from inside an MCP tool's AsyncTask.
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *SourceFilePath) || FileData.Num() == 0)
	{
		OutError = FString::Printf(TEXT("Failed to read file: %s"), *SourceFilePath);
		return FString();
	}

	const FString PackageName = Folder / FinalName;
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Failed to create package: %s"), *PackageName);
		return FString();
	}
	Package->FullyLoad();

	UTextureFactory* Factory = NewObject<UTextureFactory>();
	Factory->AddToRoot();
	UTextureFactory::SuppressImportOverwriteDialog();

	const uint8* BufferStart = FileData.GetData();
	const uint8* BufferEnd   = BufferStart + FileData.Num();

	UObject* NewObj = Factory->FactoryCreateBinary(
		UTexture2D::StaticClass(),
		Package,
		FName(*FinalName),
		RF_Public | RF_Standalone,
		nullptr,
		*Ext,
		BufferStart,
		BufferEnd,
		GWarn);

	Factory->RemoveFromRoot();

	if (!NewObj)
	{
		OutError = FString::Printf(TEXT("Texture factory failed to import '%s'"), *SourceFilePath);
		return FString();
	}

	FAssetRegistryModule::AssetCreated(NewObj);
	Package->MarkPackageDirty();
	if (!UEditorAssetLibrary::SaveLoadedAsset(NewObj, false))
	{
		OutError = FString::Printf(TEXT("Failed to save imported asset '%s'"), *NewObj->GetPathName());
		return FString();
	}

	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::ImportAsset: imported '%s' -> '%s'"), *SourceFilePath, *NewObj->GetPathName());
	return NewObj->GetPathName();
}

bool UAssetDiscoveryService::ReimportAsset(
	const FString& AssetPath,
	const FString& NewSourcePath,
	FString& OutSourceFileUsed,
	FString& OutError)
{
	OutSourceFileUsed.Empty();
	OutError.Empty();

	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("AssetPath is required");
		return false;
	}

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		OutError = FString::Printf(TEXT("Asset was not found: %s"), *AssetPath);
		return false;
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		OutError = FString::Printf(TEXT("Asset was not found or could not be loaded: %s"), *AssetPath);
		return false;
	}

	FReimportManager* ReimportManager = FReimportManager::Instance();
	if (!ReimportManager)
	{
		OutError = TEXT("Unreal's reimport manager is unavailable");
		return false;
	}

	FString ReplacementSource;
	if (!NewSourcePath.IsEmpty())
	{
		ReplacementSource = FPaths::ConvertRelativePathToFull(NewSourcePath);
		FPaths::NormalizeFilename(ReplacementSource);
		if (!FPaths::FileExists(ReplacementSource))
		{
			OutError = FString::Printf(TEXT("New source file does not exist: %s"), *ReplacementSource);
			return false;
		}

		// Let the registered handler update the correct import-data representation. This
		// works for both Interchange and legacy factories without asset-type branching.
		ReimportManager->UpdateReimportPath(Asset, ReplacementSource, INDEX_NONE);
	}

	TArray<FString> SourceFiles;
	if (!ReimportManager->CanReimport(Asset, &SourceFiles))
	{
		OutError = FString::Printf(
			TEXT("No registered reimport handler supports asset '%s'%s"),
			*AssetPath,
			ReplacementSource.IsEmpty() ? TEXT("") : TEXT(" with the supplied source file"));
		return false;
	}

	if (SourceFiles.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Asset has no stored source file: %s"), *AssetPath);
		return false;
	}

	// Report the selected source even when validation or the handler later fails. This makes
	// failure responses actionable, especially for assets whose stored source has moved.
	OutSourceFileUsed = ReplacementSource.IsEmpty()
		? UAssetImportData::ResolveImportFilename(SourceFiles[0], Asset->GetOutermost())
		: ReplacementSource;
	FPaths::NormalizeFilename(OutSourceFileUsed);

	for (const FString& SourceFile : SourceFiles)
	{
		if (SourceFile.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Asset has an empty stored source file: %s"), *AssetPath);
			return false;
		}
		FString ResolvedSourceFile = UAssetImportData::ResolveImportFilename(SourceFile, Asset->GetOutermost());
		FPaths::NormalizeFilename(ResolvedSourceFile);
		if (!FPaths::FileExists(ResolvedSourceFile))
		{
			OutError = FString::Printf(TEXT("Stored source file does not exist: %s"), *ResolvedSourceFile);
			return false;
		}
	}

	const bool bReimported = ReimportManager->Reimport(
		Asset,
		/*bAskForNewFileIfMissing=*/ false,
		/*bShowNotification=*/ false,
		/*PreferredReimportFile=*/ TEXT(""),
		/*SpecifiedReimportHandler=*/ nullptr,
		/*SourceFileIndex=*/ INDEX_NONE,
		/*bForceNewFile=*/ false,
		/*bAutomated=*/ true);

	if (!bReimported)
	{
		OutError = FString::Printf(
			TEXT("Reimport failed for asset '%s' using source file '%s'. See the Unreal log for handler details."),
			*AssetPath,
			*OutSourceFileUsed);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::ReimportAsset: reimported '%s' from '%s'"),
		*AssetPath, *OutSourceFileUsed);
	return true;
}

bool UAssetDiscoveryService::ExportTexture(const FString& AssetPath, const FString& ExportFilePath)
{
	if (AssetPath.IsEmpty() || ExportFilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::ExportTexture: AssetPath or ExportFilePath is empty"));
		return false;
	}

	// Load the texture
	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UTexture2D* Texture = Cast<UTexture2D>(LoadedAsset);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::ExportTexture: Failed to load texture: %s"), *AssetPath);
		return false;
	}

	// Use Unreal's built-in export via asset tools
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	// Get the export path directory
	FString ExportDir = FPaths::GetPath(ExportFilePath);
	
	// Ensure directory exists
	if (!FPaths::DirectoryExists(ExportDir))
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*ExportDir);
	}

	// Export the asset
	TArray<UObject*> AssetsToExport;
	AssetsToExport.Add(Texture);
	
	AssetTools.ExportAssets(AssetsToExport, ExportDir);

	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::ExportTexture: Exported texture to %s"), *ExportDir);
	return true;
}

// ========== Open Assets & Content Browser ==========

TArray<FAssetData> UAssetDiscoveryService::GetContentBrowserSelections()
{
	TArray<FAssetData> SelectedAssets;

	// Get the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	IContentBrowserSingleton& ContentBrowser = ContentBrowserModule.Get();

	// Get selected assets
	ContentBrowser.GetSelectedAssets(SelectedAssets);

	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::GetContentBrowserSelections: Found %d selected assets"), SelectedAssets.Num());
	return SelectedAssets;
}

bool UAssetDiscoveryService::GetPrimaryContentBrowserSelection(FAssetData& OutAsset)
{
	TArray<FAssetData> SelectedAssets = GetContentBrowserSelections();
	
	if (SelectedAssets.Num() > 0)
	{
		OutAsset = SelectedAssets[0];
		UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::GetPrimaryContentBrowserSelection: %s"), *OutAsset.AssetName.ToString());
		return true;
	}

	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::GetPrimaryContentBrowserSelection: No assets selected"));
	return false;
}

bool UAssetDiscoveryService::IsAssetOpen(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::IsAssetOpen: AssetPath is empty"));
		return false;
	}

	if (!GEditor)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService::IsAssetOpen: GEditor is null"));
		return false;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService::IsAssetOpen: Failed to get AssetEditorSubsystem"));
		return false;
	}

	// Load the asset to get its UObject
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::IsAssetOpen: Asset not found: %s"), *AssetPath);
		return false;
	}

	// Check if any editor is open for this asset
	TArray<IAssetEditorInstance*> Editors = AssetEditorSubsystem->FindEditorsForAsset(Asset);
	bool bIsOpen = Editors.Num() > 0;

	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::IsAssetOpen: %s is %s"), *AssetPath, bIsOpen ? TEXT("open") : TEXT("closed"));
	return bIsOpen;
}

FUnattendedDeleteResult UAssetDiscoveryService::DeleteAssetUnattended(const FString& AssetPath, bool bForceEvenIfReferenced)
{
	FUnattendedDeleteResult Result;
	if (AssetPath.IsEmpty())
	{
		Result.ErrorMessage = TEXT("AssetPath is empty");
		return Result;
	}
	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Asset not found: %s"), *AssetPath);
		return Result;
	}

	// Who points at it (the question the modal dialog would have asked the human)
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
	TArray<FName> ReferencerNames;
	AssetRegistryModule.Get().GetReferencers(FName(*PackageName), ReferencerNames);
	for (const FName& Referencer : ReferencerNames)
	{
		const FString ReferencerString = Referencer.ToString();
		if (ReferencerString != PackageName && !ReferencerString.StartsWith(TEXT("/Temp/")) && !ReferencerString.StartsWith(TEXT("/Engine/Transient")))
		{
			Result.Referencers.Add(ReferencerString);
		}
	}
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath);
		return Result;
	}

	// ForceDeleteObjects can still open a read-only-package prompt even when confirmation is
	// disabled. Preflight the resolved package file so this API keeps its unattended/no-modal
	// contract instead of wedging the game thread on a source-control or filesystem attribute.
	FString PackageFilename;
	if (FPackageName::DoesPackageExist(PackageName, &PackageFilename) &&
		FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*PackageFilename))
	{
		Result.ErrorMessage = FString::Printf(
			TEXT("Asset package is read-only; refusing unattended delete to avoid a modal prompt: %s"),
			*PackageFilename);
		return Result;
	}

	// The registry lags a freshly saved referencer (a montage built on this clip seconds ago is
	// not in its dependency map yet), so also ask memory: every loaded asset package that holds
	// a pointer to this object counts. Transient / compiled-in outers are the Python wrapper and
	// the editor itself, not references worth refusing over.
	const TArray<UObject*> Referencees = { Asset };
	for (UObject* Referencer : FReferencerFinder::GetAllReferencers(Referencees, nullptr))
	{
		UPackage* Package = Referencer ? Referencer->GetOutermost() : nullptr;
		if (!Package || Package == Asset->GetOutermost() || Package == GetTransientPackage() || Package->HasAnyPackageFlags(PKG_CompiledIn))
		{
			continue;
		}
		const FString ReferencerPackage = Package->GetName();
		if (ReferencerPackage.StartsWith(TEXT("/Game/")) || ReferencerPackage.StartsWith(TEXT("/Engine/")) || FPackageName::IsValidLongPackageName(ReferencerPackage))
		{
			if (!ReferencerPackage.StartsWith(TEXT("/Temp/")) && !ReferencerPackage.StartsWith(TEXT("/Engine/Transient")))
			{
				Result.Referencers.AddUnique(ReferencerPackage);
			}
		}
	}
	if (Result.Referencers.Num() > 0 && !bForceEvenIfReferenced)
	{
		Result.ErrorMessage = FString::Printf(TEXT("%s is referenced by %d asset(s); pass bForceEvenIfReferenced to delete anyway and clear the references"), *AssetPath, Result.Referencers.Num());
		return Result;
	}
	// Close any editor showing it first, or the delete is refused
	if (GEditor)
	{
		if (UAssetEditorSubsystem* AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetEditors->CloseAllEditorsForAsset(Asset);
		}
	}

	// A13 follow-up: mirror what the engine's own delete does BEFORE its in-memory referencer check.
	// FBlueprintActionDatabase roots a set of transient UBlueprintNodeSpawner objects (variable /
	// function / event spawners) for every loaded Blueprint, kept alive by its AddReferencedObjects
	// (BlueprintActionDatabase.cpp:1221-1238). Those spawners hold a pointer back to the Blueprint, so
	// GatherObjectReferencersForDeletion would report them and we would refuse a perfectly deletable
	// Blueprint. Inside ObjectTools::ForceDeleteObjects the engine avoids exactly this: it broadcasts
	// FEditorDelegates::OnAssetsPreDelete (ObjectTools.cpp:3978) *before* DeleteSingleObject's gather
	// (ObjectTools.cpp:3500), and FBlueprintActionDatabase::OnAssetsPendingDelete
	// (BlueprintActionDatabase.cpp:985-1014) responds by calling ClearAssetActions on the deleting
	// object. ClearAssetActions(UBlueprint) drops the single entry that holds BOTH the blueprint-graph
	// spawners and the skeleton-class member spawners (RefreshAssetActions:1656-1660). We do the same
	// here, then let the CollectGarbage below actually reap the now-unreferenced spawners before we
	// gather. On a refusal we rebuild the entry so the editor's palette is left intact.
	bool bClearedActionDatabase = false;
	if (FBlueprintActionDatabase* ActionDatabase = FBlueprintActionDatabase::TryGet())
	{
		bClearedActionDatabase = ActionDatabase->ClearAssetActions(Asset);
		if (!bClearedActionDatabase)
		{
			// A non-Blueprint asset can still own a Blueprint (matches the engine's own fallback branch,
			// BlueprintActionDatabase.cpp:1006-1013).
			if (const IBlueprintAssetHandler* Handler = FBlueprintAssetHandler::Get().FindHandler(Asset->GetClass()))
			{
				if (UBlueprint* OwnedBlueprint = Handler->RetrieveBlueprint(Asset))
				{
					bClearedActionDatabase = ActionDatabase->ClearAssetActions(OwnedBlueprint);
				}
			}
		}
	}
	// Rebuild the action-database entry we cleared, so refusing the delete does not leave the loaded
	// Blueprint's palette actions empty until its next compile/reload.
	auto RestoreActionDatabase = [&]()
	{
		if (bClearedActionDatabase && IsValid(Asset))
		{
			if (FBlueprintActionDatabase* ActionDatabase = FBlueprintActionDatabase::TryGet())
			{
				ActionDatabase->RefreshAssetActions(Asset);
			}
		}
	};

	// The refusal DECISION must be strictly NON-MUTATING. A previous shape ran ForceReplaceReferences
	// here to mirror the engine before its check; on a refusal that left the Blueprint's skeleton/generated
	// classes with a null ClassGeneratedBy, and the caller's retry (after releasing the global) then
	// crashed: "Fatal error: UBlueprintGeneratedClass::GetAuthoritativeClass: ClassGeneratedBy is null"
	// (BlueprintGeneratedClass.cpp:686). So we only LOOK before deciding, and never replace.
	//
	// ClearAssetActions above already dropped the transient Blueprint-palette node spawners (harmless —
	// mirrors the engine's OnAssetsPreDelete handler). Collect garbage the same way the engine does so a
	// global that was already del'd but not yet swept does not cause a false refusal; the asset carries
	// RF_Standalone (a GARBAGE_COLLECTION_KEEPFLAGS flag) so it survives the sweep.
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	if (!IsValid(Asset))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Asset %s was garbage-collected before deletion could proceed"), *AssetPath);
		return Result;
	}

	// Look at who holds the asset WITHOUT mutating anything (default flags, no replace first). The only
	// referencer that would make the engine's force delete stall on its modal "is in use" dialog is a
	// NATIVE GC root holding the asset directly — a UGCObjectReferencer, which is what a Python
	// module-level global manifests as. Everything else the engine's own force delete clears without
	// prompting: on-disk asset references (it nulls them via ForceReplaceReferences), the action
	// database's transient node spawners, and transient editor helpers like AnimSequencerController
	// (unrooted once the reflected refs are gone). Observed in the editor (2026-09-12): the held
	// duplicated Blueprint listed GCObjectReferencer + 2 node spawners (refuse); after releasing the
	// Python global it listed only the 2 spawners (deletes fine); an anim sequence listed only its
	// AnimSequencerController (deletes fine). So the conservative, correct signal is: refuse iff some
	// external referencer is a UGCObjectReferencer.
	{
		FReferencerInformationList Refs;
		bool bIsReferenced = false;
		bool bIsReferencedByUndo = false;
		ObjectTools::GatherObjectReferencersForDeletion(Asset, bIsReferenced, bIsReferencedByUndo, &Refs);

		TArray<FString> NativeRoots;
		for (const FReferencerInformation& Info : Refs.ExternalReferences)
		{
			if (Info.Referencer && Info.Referencer->IsA<UGCObjectReferencer>())
			{
				NativeRoots.AddUnique(Info.Referencer->GetFullName());
			}
		}

		if (NativeRoots.Num() > 0)
		{
			for (const FString& Ref : NativeRoots)
			{
				Result.Referencers.AddUnique(Ref);
			}
			Result.ErrorMessage = FString::Printf(
				TEXT("%s is held by %d native GC root(s) (e.g. %s); in an unattended session that is a Python ")
				TEXT("module-level global that created or loaded the asset. The engine's force delete would stall ")
				TEXT("on its modal 'is in use' dialog, so this refuses instead of deleting. Release the Python ")
				TEXT("globals holding it (del them, then unreal.SystemLibrary.collect_garbage()) and retry."),
				*AssetPath, NativeRoots.Num(), *NativeRoots.Last());
			UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::DeleteAssetUnattended: %s"), *Result.ErrorMessage);
			RestoreActionDatabase();
			return Result;
		}
	}

	// No native root holds it: hand the deletion to the engine's real force delete, exactly as the
	// pre-A13 code did. ForceDeleteObjects nulls the reflected references itself (on-disk data-asset
	// pointers etc.), reparents child Blueprints, removes child redirectors / generated classes and
	// reinstances UUserDefinedStructs, and clears the transient helpers — none of which makes it prompt,
	// because nothing rooted-and-native holds the asset (we just checked).
	TArray<UObject*> Objects;
	Objects.Add(Asset);
	const int32 Deleted = ObjectTools::ForceDeleteObjects(Objects, /*bShowConfirmation*/ false);
	if (Deleted <= 0)
	{
		Result.ErrorMessage = FString::Printf(TEXT("ForceDeleteObjects returned 0 for %s (system veto; see the log)"), *AssetPath);
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::DeleteAssetUnattended: %s"), *Result.ErrorMessage);
		RestoreActionDatabase();
		return Result;
	}

	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::DeleteAssetUnattended: deleted %s (%d referencer(s) reported)"), *AssetPath, Result.Referencers.Num());
	Result.bSuccess = true;
	return Result;
}
