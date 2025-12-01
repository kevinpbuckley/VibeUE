// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Services/Asset/AssetLifecycleService.h"
#include "Core/ErrorCodes.h"
#include "EditorAssetLibrary.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor/EditorEngine.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "FileHelpers.h"

FAssetLifecycleService::FAssetLifecycleService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

FString FAssetLifecycleService::NormalizeAssetPath(const FString& AssetPath) const
{
    FString NormalizedPath = AssetPath;
    
    // Add /Game prefix if it's a relative path
    if (!NormalizedPath.StartsWith(TEXT("/Game")) && 
        !NormalizedPath.StartsWith(TEXT("/Engine")) && 
        !NormalizedPath.StartsWith(TEXT("/Script")))
    {
        if (!NormalizedPath.StartsWith(TEXT("/")))
        {
            NormalizedPath = TEXT("/Game/") + NormalizedPath;
        }
        else
        {
            NormalizedPath = TEXT("/Game") + NormalizedPath;
        }
    }
    
    return NormalizedPath;
}

UObject* FAssetLifecycleService::LoadAsset(const FString& AssetPath) const
{
    FString NormalizedPath = NormalizeAssetPath(AssetPath);
    return UEditorAssetLibrary::LoadAsset(NormalizedPath);
}

FString FAssetLifecycleService::GetEditorTypeName(UObject* Asset) const
{
    if (!Asset)
    {
        return TEXT("Unknown");
    }
    
    if (Asset->IsA<UTexture>())
    {
        return TEXT("Texture Editor");
    }
    else if (Asset->IsA<UMaterial>() || Asset->IsA<UMaterialInstance>())
    {
        return TEXT("Material Editor");
    }
    else if (Asset->IsA<UBlueprint>())
    {
        return TEXT("Blueprint Editor");
    }
    else if (Asset->IsA<UStaticMesh>())
    {
        return TEXT("Static Mesh Editor");
    }
    else if (Asset->IsA<USoundBase>())
    {
        return TEXT("Audio Editor");
    }
    else if (Asset->IsA<UDataTable>())
    {
        return TEXT("Data Table Editor");
    }
    
    return Asset->GetClass()->GetName() + TEXT(" Editor");
}

TResult<FString> FAssetLifecycleService::OpenAssetInEditor(const FString& AssetPath, bool bForceOpen)
{
    FString NormalizedPath = NormalizeAssetPath(AssetPath);
    
    // Load the asset
    UObject* Asset = LoadAsset(NormalizedPath);
    if (!Asset)
    {
        // Try with different extensions
        TArray<FString> Extensions = {TEXT(".uasset"), TEXT("")};
        bool bFoundAsset = false;
        
        for (const FString& Extension : Extensions)
        {
            FString TestPath = NormalizedPath + Extension;
            Asset = UEditorAssetLibrary::LoadAsset(TestPath);
            if (Asset)
            {
                NormalizedPath = TestPath;
                bFoundAsset = true;
                break;
            }
        }
        
        if (!bFoundAsset)
        {
            return TResult<FString>::Error(
                VibeUE::ErrorCodes::ASSET_NOT_FOUND,
                FString::Printf(TEXT("Asset not found: %s"), *NormalizedPath)
            );
        }
    }
    
    // Special handling for World/Level assets - use FEditorFileUtils instead of AssetEditorSubsystem
    if (UWorld* World = Cast<UWorld>(Asset))
    {
        // Get the package name for the level
        FString PackageName = Asset->GetOutermost()->GetName();
        
        // Use FEditorFileUtils to load the map properly
        FString MapFilePath = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetMapPackageExtension());
        
        bool bSuccess = FEditorFileUtils::LoadMap(PackageName, false, true);
        
        if (bSuccess)
        {
            return TResult<FString>::Success(TEXT("Level Editor"));
        }
        else
        {
            return TResult<FString>::Error(
                VibeUE::ErrorCodes::OPERATION_FAILED,
                FString::Printf(TEXT("Failed to load level: %s"), *NormalizedPath)
            );
        }
    }
    
    // Get the Asset Editor Subsystem for non-level assets
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::INTERNAL_ERROR,
            TEXT("Failed to get Asset Editor Subsystem")
        );
    }
    
    // Check if asset is already open
    bool bWasAlreadyOpen = AssetEditorSubsystem->FindEditorForAsset(Asset, false) != nullptr;
    
    // Close if force open is requested
    if (bForceOpen && bWasAlreadyOpen)
    {
        AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
    }
    
    // Open the asset
    bool bSuccess = AssetEditorSubsystem->OpenEditorForAsset(Asset);
    
    // Verify it's actually open (sometimes OpenEditorForAsset returns false but still opens)
    bool bIsNowOpen = AssetEditorSubsystem->FindEditorForAsset(Asset, false) != nullptr;
    
    if (!bSuccess && !bIsNowOpen)
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::OPERATION_FAILED,
            FString::Printf(TEXT("Failed to open asset: %s"), *NormalizedPath)
        );
    }
    
    FString EditorType = GetEditorTypeName(Asset);
    return TResult<FString>::Success(EditorType);
}

TResult<bool> FAssetLifecycleService::IsAssetOpen(const FString& AssetPath)
{
    FString NormalizedPath = NormalizeAssetPath(AssetPath);
    
    UObject* Asset = LoadAsset(NormalizedPath);
    if (!Asset)
    {
        return TResult<bool>::Error(
            VibeUE::ErrorCodes::ASSET_NOT_FOUND,
            FString::Printf(TEXT("Asset not found: %s"), *NormalizedPath)
        );
    }
    
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
        return TResult<bool>::Error(
            VibeUE::ErrorCodes::INTERNAL_ERROR,
            TEXT("Failed to get Asset Editor Subsystem")
        );
    }
    
    bool bIsOpen = AssetEditorSubsystem->FindEditorForAsset(Asset, false) != nullptr;
    return TResult<bool>::Success(bIsOpen);
}

TResult<void> FAssetLifecycleService::CloseAsset(const FString& AssetPath)
{
    FString NormalizedPath = NormalizeAssetPath(AssetPath);
    
    UObject* Asset = LoadAsset(NormalizedPath);
    if (!Asset)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::ASSET_NOT_FOUND,
            FString::Printf(TEXT("Asset not found: %s"), *NormalizedPath)
        );
    }
    
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::INTERNAL_ERROR,
            TEXT("Failed to get Asset Editor Subsystem")
        );
    }
    
    AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
    return TResult<void>::Success();
}

TResult<void> FAssetLifecycleService::SaveAsset(const FString& AssetPath)
{
    FString NormalizedPath = NormalizeAssetPath(AssetPath);
    
    if (!UEditorAssetLibrary::DoesAssetExist(NormalizedPath))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::ASSET_NOT_FOUND,
            FString::Printf(TEXT("Asset not found: %s"), *NormalizedPath)
        );
    }
    
    bool bSuccess = UEditorAssetLibrary::SaveAsset(NormalizedPath, true);
    if (!bSuccess)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::OPERATION_FAILED,
            FString::Printf(TEXT("Failed to save asset: %s"), *NormalizedPath)
        );
    }
    
    return TResult<void>::Success();
}

TResult<int32> FAssetLifecycleService::SaveAllDirtyAssets(bool bPromptUserToSave)
{
    // Get all dirty packages
    TArray<UPackage*> DirtyPackages;
    FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);
    FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);
    
    if (DirtyPackages.Num() == 0)
    {
        // No dirty assets to save
        return TResult<int32>::Success(0);
    }
    
    // Save the packages
    bool bSuccess = false;
    if (bPromptUserToSave)
    {
        // Show save dialog to user
        bSuccess = FEditorFileUtils::SaveDirtyPackages(
            /*bPromptUserToSave=*/true,
            /*bSaveMapPackages=*/true,
            /*bSaveContentPackages=*/true,
            /*bFastSave=*/false,
            /*bNotifyNoPackagesSaved=*/true,
            /*bCanBeDeclined=*/true
        );
    }
    else
    {
        // Save without prompting
        bSuccess = FEditorFileUtils::SaveDirtyPackages(
            /*bPromptUserToSave=*/false,
            /*bSaveMapPackages=*/true,
            /*bSaveContentPackages=*/true,
            /*bFastSave=*/false,
            /*bNotifyNoPackagesSaved=*/false,
            /*bCanBeDeclined=*/false
        );
    }
    
    if (!bSuccess)
    {
        return TResult<int32>::Error(
            VibeUE::ErrorCodes::OPERATION_FAILED,
            TEXT("Failed to save all dirty assets")
        );
    }
    
    return TResult<int32>::Success(DirtyPackages.Num());
}

TResult<bool> FAssetLifecycleService::DeleteAsset(
    const FString& AssetPath,
    bool bForceDelete,
    bool bShowConfirmation
)
{
    FString NormalizedPath = NormalizeAssetPath(AssetPath);
    
    // 1. Validate asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(NormalizedPath))
    {
        return TResult<bool>::Error(
            VibeUE::ErrorCodes::ASSET_NOT_FOUND,
            FString::Printf(TEXT("Asset not found: %s"), *NormalizedPath)
        );
    }
    
    // 2. Check if asset is in engine content (read-only)
    if (NormalizedPath.StartsWith(TEXT("/Engine/")))
    {
        return TResult<bool>::Error(
            VibeUE::ErrorCodes::ASSET_READ_ONLY,
            FString::Printf(TEXT("Cannot delete engine content: %s"), *NormalizedPath)
        );
    }
    
    // 3. Check for asset references using AssetRegistry
    IAssetRegistry* AssetRegistry = GetContext()->GetAssetRegistry();
    if (!AssetRegistry)
    {
        return TResult<bool>::Error(
            VibeUE::ErrorCodes::INTERNAL_ERROR,
            TEXT("Failed to get Asset Registry")
        );
    }
    
    // Convert asset path to package name for reference checking
    FString PackageName = NormalizedPath;
    
    // Get all assets that reference this asset
    TArray<FName> Referencers;
    AssetRegistry->GetReferencers(*PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::Package);
    
    // Filter out self-references
    Referencers.Remove(*PackageName);
    
    // If asset has references and force_delete is false, return error
    if (Referencers.Num() > 0 && !bForceDelete)
    {
        // Convert TArray<FName> to comma-separated string
        TArray<FString> ReferencerStrings;
        for (const FName& Referencer : Referencers)
        {
            ReferencerStrings.Add(Referencer.ToString());
        }
        FString ReferencersStr = FString::Join(ReferencerStrings, TEXT(", "));
        
        return TResult<bool>::Error(
            VibeUE::ErrorCodes::ASSET_IN_USE,
            FString::Printf(
                TEXT("Asset has %d reference(s). Use force_delete=true to override. References: %s"),
                Referencers.Num(),
                *ReferencersStr
            )
        );
    }
    
    // 4. Optional confirmation dialog
    if (bShowConfirmation)
    {
        FText DialogTitle = FText::FromString(TEXT("Confirm Asset Deletion"));
        FText DialogMessage;
        
        if (Referencers.Num() > 0)
        {
            DialogMessage = FText::FromString(FString::Printf(
                TEXT("Delete asset '%s'?\n\nWarning: Asset has %d reference(s) that will be broken."),
                *NormalizedPath,
                Referencers.Num()
            ));
        }
        else
        {
            DialogMessage = FText::FromString(FString::Printf(
                TEXT("Delete asset '%s'?"),
                *NormalizedPath
            ));
        }
        
        EAppReturnType::Type UserResponse = FMessageDialog::Open(
            EAppMsgType::YesNo,
            DialogMessage,
            DialogTitle
        );
        
        if (UserResponse != EAppReturnType::Yes)
        {
            return TResult<bool>::Error(
                VibeUE::ErrorCodes::OPERATION_CANCELLED,
                TEXT("User cancelled deletion")
            );
        }
    }
    
    // 5. Perform deletion
    bool bSuccess = UEditorAssetLibrary::DeleteAsset(NormalizedPath);
    if (!bSuccess)
    {
        return TResult<bool>::Error(
            VibeUE::ErrorCodes::ASSET_DELETE_FAILED,
            FString::Printf(TEXT("Failed to delete asset: %s"), *NormalizedPath)
        );
    }
    
    return TResult<bool>::Success(true);
}

TResult<bool> FAssetLifecycleService::DoesAssetExist(const FString& AssetPath)
{
    FString NormalizedPath = NormalizeAssetPath(AssetPath);
    bool bExists = UEditorAssetLibrary::DoesAssetExist(NormalizedPath);
    return TResult<bool>::Success(bExists);
}

TResult<FAssetDuplicateResult> FAssetLifecycleService::DuplicateAsset(
    const FString& SourceAssetPath,
    const FString& DestinationPath,
    const FString& NewName
)
{
    // Normalize source path
    FString NormalizedSourcePath = NormalizeAssetPath(SourceAssetPath);
    
    // 1. Validate source asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(NormalizedSourcePath))
    {
        return TResult<FAssetDuplicateResult>::Error(
            VibeUE::ErrorCodes::ASSET_NOT_FOUND,
            FString::Printf(TEXT("Source asset not found: %s"), *NormalizedSourcePath)
        );
    }
    
    // 2. Normalize and validate destination path
    FString NormalizedDestPath = NormalizeAssetPath(DestinationPath);
    
    // Ensure destination path exists as a directory
    if (!UEditorAssetLibrary::DoesDirectoryExist(NormalizedDestPath))
    {
        return TResult<FAssetDuplicateResult>::Error(
            VibeUE::ErrorCodes::INVALID_PATH,
            FString::Printf(TEXT("Destination directory does not exist: %s"), *NormalizedDestPath)
        );
    }
    
    // 3. Determine the new asset name
    FString FinalNewName = NewName;
    if (FinalNewName.IsEmpty())
    {
        // Extract the asset name from source path using FPaths
        FinalNewName = FPaths::GetBaseFilename(NormalizedSourcePath);
    }
    
    // 4. Build the destination asset path
    FString DestinationAssetPath = NormalizedDestPath;
    if (!DestinationAssetPath.EndsWith(TEXT("/")))
    {
        DestinationAssetPath += TEXT("/");
    }
    DestinationAssetPath += FinalNewName;
    
    // 5. Duplicate the asset using UEditorAssetLibrary
    UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(
        NormalizedSourcePath,
        DestinationAssetPath
    );
    
    if (!DuplicatedAsset)
    {
        return TResult<FAssetDuplicateResult>::Error(
            VibeUE::ErrorCodes::OPERATION_FAILED,
            FString::Printf(
                TEXT("Failed to duplicate asset from '%s' to '%s'"),
                *NormalizedSourcePath,
                *DestinationAssetPath
            )
        );
    }
    
    // 6. Build result
    FAssetDuplicateResult Result;
    Result.OriginalPath = NormalizedSourcePath;
    Result.NewPath = DestinationAssetPath;
    Result.AssetType = DuplicatedAsset->GetClass()->GetName();
    
    return TResult<FAssetDuplicateResult>::Success(Result);
}

TResult<FAssetReferencesResult> FAssetLifecycleService::GetAssetReferences(
    const FString& AssetPath,
    bool bIncludeReferencers,
    bool bIncludeDependencies)
{
    FString NormalizedPath = NormalizeAssetPath(AssetPath);
    
    // 1. Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(NormalizedPath))
    {
        return TResult<FAssetReferencesResult>::Error(
            VibeUE::ErrorCodes::ASSET_NOT_FOUND,
            FString::Printf(TEXT("Asset not found: %s"), *NormalizedPath)
        );
    }
    
    // 2. Get AssetRegistry
    IAssetRegistry* AssetRegistry = GetContext()->GetAssetRegistry();
    if (!AssetRegistry)
    {
        return TResult<FAssetReferencesResult>::Error(
            VibeUE::ErrorCodes::INTERNAL_ERROR,
            TEXT("Failed to get Asset Registry")
        );
    }
    
    FAssetReferencesResult Result;
    Result.AssetPath = NormalizedPath;
    
    // 3. Get referencers (assets that reference this asset)
    if (bIncludeReferencers)
    {
        TArray<FName> ReferencerPackages;
        AssetRegistry->GetReferencers(*NormalizedPath, ReferencerPackages, UE::AssetRegistry::EDependencyCategory::Package);
        
        // Filter out self-references
        ReferencerPackages.Remove(*NormalizedPath);
        
        for (const FName& PackageName : ReferencerPackages)
        {
            FString PackagePath = PackageName.ToString();
            
            // Get asset data for this package
            TArray<FAssetData> AssetsInPackage;
            AssetRegistry->GetAssetsByPackageName(PackageName, AssetsInPackage);
            
            for (const FAssetData& AssetData : AssetsInPackage)
            {
                FAssetReferenceInfo RefInfo;
                RefInfo.AssetPath = AssetData.GetObjectPathString();
                RefInfo.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
                RefInfo.DisplayName = AssetData.AssetName.ToString();
                Result.Referencers.Add(RefInfo);
            }
        }
        Result.ReferencerCount = Result.Referencers.Num();
    }
    
    // 4. Get dependencies (assets this asset references)
    if (bIncludeDependencies)
    {
        TArray<FName> DependencyPackages;
        AssetRegistry->GetDependencies(*NormalizedPath, DependencyPackages, UE::AssetRegistry::EDependencyCategory::Package);
        
        // Filter out self-references and engine/script packages
        DependencyPackages.RemoveAll([&NormalizedPath](const FName& PackageName) {
            FString PackagePath = PackageName.ToString();
            return PackagePath == NormalizedPath || 
                   PackagePath.StartsWith(TEXT("/Engine/")) ||
                   PackagePath.StartsWith(TEXT("/Script/"));
        });
        
        for (const FName& PackageName : DependencyPackages)
        {
            // Get asset data for this package
            TArray<FAssetData> AssetsInPackage;
            AssetRegistry->GetAssetsByPackageName(PackageName, AssetsInPackage);
            
            for (const FAssetData& AssetData : AssetsInPackage)
            {
                FAssetReferenceInfo RefInfo;
                RefInfo.AssetPath = AssetData.GetObjectPathString();
                RefInfo.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
                RefInfo.DisplayName = AssetData.AssetName.ToString();
                Result.Dependencies.Add(RefInfo);
            }
        }
        Result.DependencyCount = Result.Dependencies.Num();
    }
    
    return TResult<FAssetReferencesResult>::Success(Result);
}
