// Copyright Buckley Builds LLC 2025 All Rights Reserved.

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
	FARFilter Filter;

	// Apply type filter if specified
	if (!AssetType.IsEmpty())
	{
		FTopLevelAssetPath ClassPath = GetAssetClassPath(AssetType);
		if (!ClassPath.IsNull())
		{
			Filter.ClassPaths.Add(ClassPath);
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

	// Try as package name first
	FAssetData FoundAsset = AssetRegistry->GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	if (FoundAsset.IsValid())
	{
		OutAsset = FoundAsset;
		return true;
	}

	// Try searching by asset name if direct path failed
	TArray<FAssetData> AllAssets;
	AssetRegistry->GetAssetsByPackageName(FName(*AssetPath), AllAssets);

	if (AllAssets.Num() > 0)
	{
		OutAsset = AllAssets[0];
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
	if (SourceFilePath.IsEmpty() || DestinationPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::ImportTexture: SourceFilePath or DestinationPath is empty"));
		return false;
	}

	if (!FPaths::FileExists(SourceFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::ImportTexture: Source file does not exist: %s"), *SourceFilePath);
		return false;
	}

	// Get asset tools
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	// Parse destination path into package path and asset name
	FString PackagePath, AssetName;
	DestinationPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (PackagePath.IsEmpty())
	{
		PackagePath = TEXT("/Game");
		AssetName = DestinationPath;
	}

	// Import the texture
	TArray<FString> FilesToImport;
	FilesToImport.Add(SourceFilePath);

	TArray<UObject*> ImportedAssets = AssetTools.ImportAssets(FilesToImport, PackagePath);
	
	if (ImportedAssets.Num() > 0 && ImportedAssets[0])
	{
		// Rename if needed
		UObject* ImportedAsset = ImportedAssets[0];
		FString CurrentName = ImportedAsset->GetName();
		if (CurrentName != AssetName && !AssetName.IsEmpty())
		{
			FString NewPath = PackagePath / AssetName;
			UEditorAssetLibrary::RenameAsset(ImportedAsset->GetPathName(), NewPath);
		}
		
		UE_LOG(LogTemp, Log, TEXT("UAssetDiscoveryService::ImportTexture: Successfully imported texture to %s"), *DestinationPath);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("UAssetDiscoveryService::ImportTexture: Failed to import texture from %s"), *SourceFilePath);
	return false;
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
