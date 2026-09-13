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

	// Steps (0)-(7) below replicate the front half of ObjectTools::ForceDeleteObjects
	// (ObjectTools.cpp:3621-4063) purely to reach the SAME refusal DECISION the engine reaches, without
	// its modal: where the engine would pop the "is in use" dialog and stall an unattended editor, we
	// refuse and return the referencer names instead. Guessing which referencers block (the earlier
	// two-gather / property-count approach) is impossible before the reference replace runs — a transient
	// helper like AnimSequencerController holds the asset NATIVELY via AddReferencedObjects, which
	// force-replace cannot null, yet the engine still deletes it because that helper is not reachable from
	// a GC root once the reflected references are gone. Only replicating the engine's replace -> GC ->
	// reachability check answers this correctly. Once that check passes, step (6) hands the ACTUAL
	// deletion to the real ObjectTools::ForceDeleteObjects so all of its asset-type fixups keep full
	// engine fidelity (see the note there); the replace+GC we already did guarantees its own internal
	// reference check cannot reach the dialog.
	//
	// (0) OnAssetsCanDelete gate (ForceDeleteObjects:3629-3635) — refuse WITHOUT a dialog if a system
	//     vetoes the delete (DeleteSingleObject would itself pop that dialog otherwise, at :3462).
	TArray<UObject*> DeleteObjects;
	DeleteObjects.Add(Asset);
	{
		FCanDeleteAssetResult CanDelete;
		FEditorDelegates::OnAssetsCanDelete.Broadcast(DeleteObjects, CanDelete);
		if (!CanDelete.Get())
		{
			Result.ErrorMessage = FString::Printf(TEXT("A system vetoed deletion of %s (OnAssetsCanDelete). See the log for details."), *AssetPath);
			UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::DeleteAssetUnattended: %s"), *Result.ErrorMessage);
			RestoreActionDatabase();
			return Result;
		}
	}
	// (2b) Pre-force-delete hook (ForceDeleteObjects:3688). (Editors were already closed above; the
	//      action database was already cleared above — that mirrors the OnAssetsPreDelete handler.)
	FEditorDelegates::OnPreForceDeleteObjects.Broadcast(DeleteObjects);

	// (3) Null every reflected reference to the asset held anywhere in memory, exactly as the engine does
	//     (ForceDeleteObjects:3957 -> the empty-within-set overload iterates a FThreadSafeObjectIterator
	//     over ALL objects, ObjectTools.cpp:1365-1371). WARNING: with bForceEvenIfReferenced this mutates
	//     other in-memory objects — a subsequent refusal leaves them with nulled references, the same
	//     state the engine's own force delete produces before its (now-replaced) dialog.
	TArray<UObject*> ObjectsToReplace;
	ObjectsToReplace.Add(Asset);
	ObjectTools::ForceReplaceReferences(nullptr, ObjectsToReplace);

	// (4) Collect garbage as the engine does (ForceDeleteObjects:3964). KEEPFLAGS keeps the RF_Standalone
	//     asset alive; helpers that lost their last reflected reference above are reaped here.
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	if (!IsValid(Asset))
	{
		// The asset itself was reaped (it should not be — RF_Standalone survives the sweep). Nothing to
		// restore, and nothing left to delete.
		Result.ErrorMessage = FString::Printf(TEXT("Asset %s was garbage-collected before deletion could proceed"), *AssetPath);
		return Result;
	}

	// (7) Give other systems the chance to drop references to the asset (ForceDeleteObjects:3974-3979).
	FEditorDelegates::OnAssetsPreDelete.Broadcast(DeleteObjects);

	// (5) Run the SAME check DeleteSingleObject runs before it would pop the dialog: the default-flags
	//     gather (ObjectTools.cpp:3500), which is reachability-filtered (FindObjectsRoots at :411-436) —
	//     it reports a referencer only if that referencer is itself reachable from a GC root and so
	//     genuinely keeps the asset alive. After the replace+GC above, a native helper that is no longer
	//     rooted drops out; a Python module-level global (a UGCObjectReferencer GC root) does not.
	{
		FReferencerInformationList Refs;
		bool bIsReferenced = false;
		bool bIsReferencedByUndo = false;
		ObjectTools::GatherObjectReferencersForDeletion(Asset, bIsReferenced, bIsReferencedByUndo, &Refs);

		// Only the undo buffer holds it -> reset the transaction buffer and proceed, exactly as the engine
		// does (DeleteSingleObject:3502-3506).
		if (!bIsReferenced && bIsReferencedByUndo && GEditor)
		{
			GEditor->ResetTransaction(NSLOCTEXT("UnrealEd", "DeleteSelectedItem", "Delete Selected Item"));
		}

		if (bIsReferenced)
		{
			// This is the exact referencer set the engine's modal dialog (ObjectTools.cpp:3513-3516) would
			// have shown. Refuse instead of prompting.
			for (const FReferencerInformation& Info : Refs.ExternalReferences)
			{
				if (Info.Referencer)
				{
					Result.Referencers.AddUnique(Info.Referencer->GetFullName());
				}
			}
			const FString Example = Result.Referencers.Num() > 0 ? Result.Referencers.Last() : Asset->GetFullName();
			Result.ErrorMessage = FString::Printf(
				TEXT("%s is still referenced after force-replace (native references) by %d rooted object(s) (e.g. %s). ")
				TEXT("This is the exact set the engine's modal 'is in use' dialog would have shown; deleting now would ")
				TEXT("stall the editor. Release any Python globals holding this object (del them, then ")
				TEXT("unreal.SystemLibrary.collect_garbage()) and retry."),
				*AssetPath, Result.Referencers.Num(), *Example);
			UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::DeleteAssetUnattended: %s"), *Result.ErrorMessage);
			RestoreActionDatabase();
			return Result;
		}
	}

	// (6) Nothing reachable from a GC root references the asset — we just proved it with the same
	//     reachability gather ForceDeleteObjects' internal DeleteSingleObject runs. So hand the actual
	//     deletion to the real ObjectTools::ForceDeleteObjects (bShowConfirmation=false) rather than a
	//     bare DeleteSingleObject: only the full path does child-Blueprint reparenting, child-redirector
	//     and generated-class removal, and UUserDefinedStruct reinstancing (ForceDeleteObjects:3722-3953),
	//     so a Blueprint/struct asset is deleted with full engine fidelity.
	//
	//     Its internal "{0} is in use" reference dialog (DeleteSingleObject:3514) CANNOT fire here:
	//     ForceDeleteObjects re-runs ForceReplaceReferences(nullptr, ...) before that check, so the
	//     reachability state is at least as clean as the one we just gathered as not-referenced. The
	//     OnAssetsCanDelete veto dialogs (ForceDeleteObjects:3633, DeleteSingleObject:3462) are pre-empted
	//     by our own OnAssetsCanDelete gate above. The remaining modal it CAN still pop is
	//     MakeReadOnlyPackageWritable (ObjectTools.cpp:3372) — only when the asset's .uasset is read-only
	//     ON DISK and source control is disabled; unattended temp/generated assets are writable, so it
	//     does not fire in practice, but a read-only on-disk file is the one case that could still stall.
	const int32 Deleted = ObjectTools::ForceDeleteObjects(DeleteObjects, /*bShowConfirmation*/ false);
	if (Deleted <= 0)
	{
		Result.ErrorMessage = FString::Printf(TEXT("ForceDeleteObjects returned 0 for %s after force-replace (a read-only on-disk package, or a system veto; see the log)"), *AssetPath);
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::DeleteAssetUnattended: %s"), *Result.ErrorMessage);
		RestoreActionDatabase();
		return Result;
	}

	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::DeleteAssetUnattended: deleted %s (%d referencer(s) reported)"), *AssetPath, Result.Referencers.Num());
	Result.bSuccess = true;
	return Result;
}
