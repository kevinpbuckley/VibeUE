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
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/MessageDialog.h"

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
    
    // Get the Asset Editor Subsystem
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

TResult<bool> FAssetLifecycleService::DeleteAsset(
    const FString& AssetPath,
    bool bForceDelete,
    bool bShowConfirmation)
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
    
    // 3. Check for references using AssetRegistry
    if (!bForceDelete)
    {
        // Load the asset to get its package name
        UObject* Asset = LoadAsset(NormalizedPath);
        if (Asset && Asset->GetPackage())
        {
            FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
            IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
            
            // Get package name from the loaded asset
            FName AssetPackageName = Asset->GetPackage()->GetFName();
            
            // Get referencers (assets that depend on this asset)
            TArray<FName> Referencers;
            AssetRegistry.GetReferencers(AssetPackageName, Referencers);
            
            // Count references excluding self-references
            int32 ExternalReferenceCount = 0;
            for (const FName& PackageName : Referencers)
            {
                if (PackageName != AssetPackageName)
                {
                    ExternalReferenceCount++;
                }
            }
            
            if (ExternalReferenceCount > 0)
            {
                return TResult<bool>::Error(
                    VibeUE::ErrorCodes::ASSET_IN_USE,
                    FString::Printf(
                        TEXT("Asset has %d references. Use force_delete=true to override."), 
                        ExternalReferenceCount
                    )
                );
            }
        }
    }
    
    // 4. Optional confirmation dialog
    if (bShowConfirmation)
    {
        EAppReturnType::Type DialogResult = FMessageDialog::Open(
            EAppMsgType::YesNo,
            FText::FromString(FString::Printf(TEXT("Delete asset '%s'?"), *NormalizedPath))
        );
        
        if (DialogResult != EAppReturnType::Yes)
        {
            return TResult<bool>::Error(
                VibeUE::ErrorCodes::OPERATION_CANCELLED,
                TEXT("User cancelled deletion")
            );
        }
    }
    
    // 5. Attempt to delete the asset
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
