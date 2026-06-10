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

namespace
{
	// Helper to get asset registry
	IAssetRegistry* GetAssetRegistry()
	{
		return &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	}

	// Helper to convert class name to FTopLevelAssetPath
	FTopLevelAssetPath GetAssetClassPath(const FString& ClassName)
	{
		if (ClassName.IsEmpty())
		{
			return FTopLevelAssetPath();
		}

		// Common asset types mapping
		static const TMap<FString, FString> ClassPathMap = {
			{TEXT("Texture2D"), TEXT("/Script/Engine.Texture2D")},
			{TEXT("Blueprint"), TEXT("/Script/Engine.Blueprint")},
			{TEXT("WidgetBlueprint"), TEXT("/Script/UMGEditor.WidgetBlueprint")},
			{TEXT("Material"), TEXT("/Script/Engine.Material")},
			{TEXT("MaterialInstance"), TEXT("/Script/Engine.MaterialInstance")},
			{TEXT("MaterialInstanceConstant"), TEXT("/Script/Engine.MaterialInstanceConstant")},
			{TEXT("StaticMesh"), TEXT("/Script/Engine.StaticMesh")},
			{TEXT("SkeletalMesh"), TEXT("/Script/Engine.SkeletalMesh")},
			{TEXT("Sound"), TEXT("/Script/Engine.SoundBase")},
			{TEXT("SoundWave"), TEXT("/Script/Engine.SoundWave")},
			{TEXT("SoundCue"), TEXT("/Script/Engine.SoundCue")},
			{TEXT("DataTable"), TEXT("/Script/Engine.DataTable")},
			{TEXT("DataAsset"), TEXT("/Script/Engine.DataAsset")},
			{TEXT("PrimaryDataAsset"), TEXT("/Script/Engine.PrimaryDataAsset")},
			{TEXT("Curve"), TEXT("/Script/Engine.CurveBase")},
			{TEXT("CurveFloat"), TEXT("/Script/Engine.CurveFloat")},
			{TEXT("ParticleSystem"), TEXT("/Script/Engine.ParticleSystem")},
			{TEXT("NiagaraSystem"), TEXT("/Script/Niagara.NiagaraSystem")},
			{TEXT("AnimSequence"), TEXT("/Script/Engine.AnimSequence")},
			{TEXT("AnimBlueprint"), TEXT("/Script/Engine.AnimBlueprint")},
			{TEXT("PhysicsAsset"), TEXT("/Script/Engine.PhysicsAsset")},
			{TEXT("Skeleton"), TEXT("/Script/Engine.Skeleton")},
		};

		if (const FString* FoundPath = ClassPathMap.Find(ClassName))
		{
			return FTopLevelAssetPath(*FoundPath);
		}

		// Accept a full "/Script/Module.Class" path directly
		if (ClassName.StartsWith(TEXT("/Script/")))
		{
			return FTopLevelAssetPath(ClassName);
		}

		// Resolve short names from any loaded module (e.g. InputAction lives in
		// /Script/EnhancedInput, LandscapeGrassType in /Script/Landscape)
		if (const UClass* FoundClass = UClass::TryFindTypeSlow<UClass>(ClassName))
		{
			return FoundClass->GetClassPathName();
		}

		// If not in map, try to construct path assuming it's in Engine
		return FTopLevelAssetPath(FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
	}
}

TArray<FAssetData> UAssetDiscoveryService::SearchAssets(const FString& SearchTerm, const FString& AssetType)
{
	IAssetRegistry* AssetRegistry = GetAssetRegistry();
	if (!AssetRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService: Failed to access Asset Registry"));
		return TArray<FAssetData>();
	}

	TArray<FAssetData> AllAssets;

	// Collect all mounted content roots (/Game, /Engine, /PluginName, …)
	// so we search Game, Engine and every plugin in one pass.
	TArray<FString> RootPaths;
	FPackageName::QueryRootContentPaths(RootPaths);

	FARFilter Filter;
	for (const FString& Root : RootPaths)
	{
		// QueryRootContentPaths returns paths with trailing '/' — FName handles both forms.
		Filter.PackagePaths.Add(FName(*Root));
	}
	Filter.bRecursivePaths = true;

	// Apply type filter if specified
	if (!AssetType.IsEmpty())
	{
		FTopLevelAssetPath ClassPath = GetAssetClassPath(AssetType);
		if (!ClassPath.IsNull())
		{
			Filter.ClassPaths.Add(ClassPath);
			Filter.bRecursiveClasses = true;
		}
	}

	// Get all assets matching the filter
	AssetRegistry->GetAssets(Filter, AllAssets);

	// If no search term, return all filtered assets
	if (SearchTerm.IsEmpty())
	{
		return AllAssets;
	}

	// Filter by search term (case-insensitive name match)
	TArray<FAssetData> MatchingAssets;
	FString LowerSearchTerm = SearchTerm.ToLower();

	for (const FAssetData& Asset : AllAssets)
	{
		FString AssetName = Asset.AssetName.ToString().ToLower();
		if (AssetName.Contains(LowerSearchTerm))
		{
			MatchingAssets.Add(Asset);
		}
	}

	return MatchingAssets;
}

TArray<FAssetData> UAssetDiscoveryService::GetAssetsByType(const FString& AssetType)
{
	if (AssetType.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::GetAssetsByType: AssetType is empty"));
		return TArray<FAssetData>();
	}

	IAssetRegistry* AssetRegistry = GetAssetRegistry();
	if (!AssetRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService: Failed to access Asset Registry"));
		return TArray<FAssetData>();
	}

	TArray<FAssetData> Assets;
	FARFilter Filter;

	FTopLevelAssetPath ClassPath = GetAssetClassPath(AssetType);
	if (!ClassPath.IsNull())
	{
		Filter.ClassPaths.Add(ClassPath);
	}

	AssetRegistry->GetAssets(Filter, Assets);
	return Assets;
}

bool UAssetDiscoveryService::FindAssetByPath(const FString& AssetPath, FAssetData& OutAsset)
{
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::FindAssetByPath: AssetPath is empty"));
		return false;
	}

	IAssetRegistry* AssetRegistry = GetAssetRegistry();
	if (!AssetRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService: Failed to access Asset Registry"));
		return false;
	}

	// Try by package name first. Users supply paths like "/Game/Folder/Asset" (no .AssetName
	// suffix), and GetAssetsByPackageName returns correctly-populated FAssetData where
	// AssetName is just "Asset" (not the full path).
	TArray<FAssetData> PackageAssets;
	AssetRegistry->GetAssetsByPackageName(FName(*AssetPath), PackageAssets);
	for (const FAssetData& Candidate : PackageAssets)
	{
		// Reject empty package shells. After a manage_asset move the asset registry can
		// retain a "/Script/CoreUObject.Package"-classed entry at the old path with no real
		// asset behind it, which would otherwise be reported as found with a malformed
		// asset_name (full path) and object_path ("<package>.<package>").
		if (Candidate.AssetClassPath == FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("Package")))
		{
			continue;
		}
		OutAsset = Candidate;
		return true;
	}

	// Fall back to object-path lookup for callers that supply the full ".AssetName" suffix
	// (e.g. "/Game/Folder/Asset.Asset").
	FAssetData FoundAsset = AssetRegistry->GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	if (FoundAsset.IsValid()
		&& FoundAsset.AssetClassPath != FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("Package")))
	{
		OutAsset = FoundAsset;
		return true;
	}

	return false;
}

TArray<FString> UAssetDiscoveryService::GetAssetDependencies(const FString& AssetPath)
{
	TArray<FString> Dependencies;

	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::GetAssetDependencies: AssetPath is empty"));
		return Dependencies;
	}

	IAssetRegistry* AssetRegistry = GetAssetRegistry();
	if (!AssetRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService: Failed to access Asset Registry"));
		return Dependencies;
	}

	// Get the asset data first
	FAssetData AssetData;
	if (!FindAssetByPath(AssetPath, AssetData))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::GetAssetDependencies: Asset not found: %s"), *AssetPath);
		return Dependencies;
	}

	// Get dependencies
	TArray<FName> DependencyNames;
	AssetRegistry->GetDependencies(AssetData.PackageName, DependencyNames);

	for (const FName& DependencyName : DependencyNames)
	{
		Dependencies.Add(DependencyName.ToString());
	}

	return Dependencies;
}

TArray<FString> UAssetDiscoveryService::GetAssetReferencers(const FString& AssetPath)
{
	TArray<FString> Referencers;

	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::GetAssetReferencers: AssetPath is empty"));
		return Referencers;
	}

	IAssetRegistry* AssetRegistry = GetAssetRegistry();
	if (!AssetRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService: Failed to access Asset Registry"));
		return Referencers;
	}

	// Get the asset data first
	FAssetData AssetData;
	if (!FindAssetByPath(AssetPath, AssetData))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::GetAssetReferencers: Asset not found: %s"), *AssetPath);
		return Referencers;
	}

	// Get referencers
	TArray<FName> ReferencerNames;
	AssetRegistry->GetReferencers(AssetData.PackageName, ReferencerNames);

	for (const FName& ReferencerName : ReferencerNames)
	{
		Referencers.Add(ReferencerName.ToString());
	}

	return Referencers;
}

TArray<FAssetData> UAssetDiscoveryService::ListAssetsInPath(const FString& Path, const FString& AssetType)
{
	if (Path.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::ListAssetsInPath: Path is empty"));
		return TArray<FAssetData>();
	}

	IAssetRegistry* AssetRegistry = GetAssetRegistry();
	if (!AssetRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService: Failed to access Asset Registry"));
		return TArray<FAssetData>();
	}

	TArray<FAssetData> Assets;
	FARFilter Filter;

	// Add path filter
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	// Add type filter if specified
	if (!AssetType.IsEmpty())
	{
		FTopLevelAssetPath ClassPath = GetAssetClassPath(AssetType);
		if (!ClassPath.IsNull())
		{
			Filter.ClassPaths.Add(ClassPath);
		}
	}

	AssetRegistry->GetAssets(Filter, Assets);
	return Assets;
}

// ========== Asset Operations ==========

bool UAssetDiscoveryService::OpenAsset(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::OpenAsset: AssetPath is empty"));
		return false;
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::OpenAsset: Failed to load asset: %s"), *AssetPath);
		return false;
	}

	if (GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			return AssetEditorSubsystem->OpenEditorForAsset(Asset);
		}
	}

	UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService::OpenAsset: Editor subsystem not available"));
	return false;
}

bool UAssetDiscoveryService::DeleteAsset(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::DeleteAsset: AssetPath is empty"));
		return false;
	}

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::DeleteAsset: Asset does not exist: %s"), *AssetPath);
		return false;
	}

	return UEditorAssetLibrary::DeleteAsset(AssetPath);
}

bool UAssetDiscoveryService::DuplicateAsset(const FString& SourcePath, const FString& DestinationPath)
{
	if (SourcePath.IsEmpty() || DestinationPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::DuplicateAsset: SourcePath or DestinationPath is empty"));
		return false;
	}

	if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::DuplicateAsset: Source asset does not exist: %s"), *SourcePath);
		return false;
	}

	return UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath) != nullptr;
}

bool UAssetDiscoveryService::MoveAsset(const FString& SourcePath, const FString& DestinationPath)
{
	if (SourcePath.IsEmpty() || DestinationPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::MoveAsset: SourcePath or DestinationPath is empty"));
		return false;
	}

	if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::MoveAsset: Source asset does not exist: %s"), *SourcePath);
		return false;
	}

	if (UEditorAssetLibrary::DoesAssetExist(DestinationPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::MoveAsset: Destination asset already exists: %s"), *DestinationPath);
		return false;
	}

	return UEditorAssetLibrary::RenameAsset(SourcePath, DestinationPath);
}

bool UAssetDiscoveryService::SaveAsset(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::SaveAsset: AssetPath is empty"));
		return false;
	}

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::SaveAsset: Asset does not exist: %s"), *AssetPath);
		return false;
	}

	return UEditorAssetLibrary::SaveAsset(AssetPath, false);
}

int32 UAssetDiscoveryService::SaveAllAssets()
{
	TArray<FString> DirtyPackages;
	
	// Get all dirty packages
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		UPackage* Package = *It;
		if (Package && Package->IsDirty() && !Package->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			FString PackageName = Package->GetName();
			// Only save game content packages
			if (PackageName.StartsWith(TEXT("/Game/")))
			{
				DirtyPackages.Add(PackageName);
			}
		}
	}

	int32 SavedCount = 0;
	for (const FString& PackageName : DirtyPackages)
	{
		if (UEditorAssetLibrary::SaveAsset(PackageName, false))
		{
			SavedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::SaveAllAssets: Saved %d assets"), SavedCount);
	return SavedCount;
}

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

// ========== Existence Checks ==========

bool UAssetDiscoveryService::AssetExists(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return false;
	}
	return UEditorAssetLibrary::DoesAssetExist(AssetPath);
}

// ========== Open Assets & Content Browser ==========

bool UAssetDiscoveryService::GetActiveAsset(FAssetData& OutAsset)
{
	if (!GEditor)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService::GetActiveAsset: GEditor is null"));
		return false;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService::GetActiveAsset: Failed to get AssetEditorSubsystem"));
		return false;
	}

	// Get all open editors and find the most recently activated one
	TArray<IAssetEditorInstance*> OpenEditors = AssetEditorSubsystem->GetAllOpenEditors();
	if (OpenEditors.Num() == 0)
	{
		// No active editor, return false
		return false;
	}

	// Find editor with most recent activation time
	IAssetEditorInstance* ActiveEditor = nullptr;
	double MostRecentTime = -1.0;
	for (IAssetEditorInstance* Editor : OpenEditors)
	{
		if (Editor)
		{
			double ActivationTime = Editor->GetLastActivationTime();
			if (ActivationTime > MostRecentTime)
			{
				MostRecentTime = ActivationTime;
				ActiveEditor = Editor;
			}
		}
	}

	if (!ActiveEditor)
	{
		return false;
	}

	// Get edited assets via the subsystem - find assets that this editor is handling
	TArray<UObject*> AllEditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
	UObject* EditedAsset = nullptr;

	// Find an asset that this editor is associated with
	for (UObject* Asset : AllEditedAssets)
	{
		TArray<IAssetEditorInstance*> EditorsForAsset = AssetEditorSubsystem->FindEditorsForAsset(Asset);
		if (EditorsForAsset.Contains(ActiveEditor))
		{
			EditedAsset = Asset;
			break;
		}
	}

	if (!EditedAsset)
	{
		return false;
	}

	IAssetRegistry* AssetRegistry = GetAssetRegistry();
	if (!AssetRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService::GetActiveAsset: Failed to access Asset Registry"));
		return false;
	}

	// Convert to FAssetData
	OutAsset = AssetRegistry->GetAssetByObjectPath(FSoftObjectPath(EditedAsset));
	if (OutAsset.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::GetActiveAsset: %s"), *OutAsset.AssetName.ToString());
		return true;
	}

	return false;
}

TArray<FAssetData> UAssetDiscoveryService::GetOpenAssets()
{
	TArray<FAssetData> OpenAssets;

	if (!GEditor)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService::GetOpenAssets: GEditor is null"));
		return OpenAssets;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService::GetOpenAssets: Failed to get AssetEditorSubsystem"));
		return OpenAssets;
	}

	// Get all currently edited assets
	TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();

	IAssetRegistry* AssetRegistry = GetAssetRegistry();
	if (!AssetRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("UAssetDiscoveryService::GetOpenAssets: Failed to access Asset Registry"));
		return OpenAssets;
	}

	// Convert UObject* to FAssetData
	for (UObject* Asset : EditedAssets)
	{
		if (Asset)
		{
			FAssetData AssetData = AssetRegistry->GetAssetByObjectPath(FSoftObjectPath(Asset));
			if (AssetData.IsValid())
			{
				OpenAssets.Add(AssetData);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::GetOpenAssets: Found %d open assets"), OpenAssets.Num());
	return OpenAssets;
}

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
