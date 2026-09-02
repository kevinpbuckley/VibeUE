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

bool UAssetDiscoveryService::DeleteAssetUnattended(const FString& AssetPath, bool bForceEvenIfReferenced, TArray<FString>& OutReferencers, FString& OutError)
{
	OutReferencers.Reset();
	OutError.Reset();
	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("AssetPath is empty");
		return false;
	}
	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		OutError = FString::Printf(TEXT("Asset not found: %s"), *AssetPath);
		return false;
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
			OutReferencers.Add(ReferencerString);
		}
	}
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		OutError = FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath);
		return false;
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
				OutReferencers.AddUnique(ReferencerPackage);
			}
		}
	}
	if (OutReferencers.Num() > 0 && !bForceEvenIfReferenced)
	{
		OutError = FString::Printf(TEXT("%s is referenced by %d asset(s); pass bForceEvenIfReferenced to delete anyway and clear the references"), *AssetPath, OutReferencers.Num());
		return false;
	}
	// Close any editor showing it first, or the delete is refused
	if (GEditor)
	{
		if (UAssetEditorSubsystem* AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetEditors->CloseAllEditorsForAsset(Asset);
		}
	}

	TArray<UObject*> Objects;
	Objects.Add(Asset);
	// Always the force path, with no confirmation: the asset-registry check above is the real
	// "is anything on disk pointing at it" gate. The plain DeleteObjects refuses (returns 0, or
	// would ask) over IN-MEMORY references — the Python variable that just created or loaded the
	// asset is enough — which is exactly the case an unattended session is always in.
	const int32 Deleted = ObjectTools::ForceDeleteObjects(Objects, /*bShowConfirmation*/ false);
	if (Deleted <= 0)
	{
		OutError = FString::Printf(TEXT("Force delete of %s returned 0 (is it open in an editor, or a Blueprint still loaded this session? see the asset-management skill)"), *AssetPath);
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::DeleteAssetUnattended: deleted %s (%d referencer(s) cleared)"), *AssetPath, OutReferencers.Num());
	return true;
}
