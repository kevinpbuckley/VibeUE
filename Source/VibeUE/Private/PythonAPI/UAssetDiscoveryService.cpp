// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UAssetDiscoveryService.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformFileManager.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Factories/TextureFactory.h"
#include "UObject/Package.h"

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
	UEditorAssetLibrary::SaveLoadedAsset(NewObj, false);

	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::ImportAsset: imported '%s' -> '%s'"), *SourceFilePath, *NewObj->GetPathName());
	return NewObj->GetPathName();
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
