// Copyright VibeUE 2025

#include "Services/Asset/AssetDiscoveryService.h"
#include "Core/ErrorCodes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

FAssetDiscoveryService::FAssetDiscoveryService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

IAssetRegistry* FAssetDiscoveryService::GetAssetRegistry() const
{
    return GetContext()->GetAssetRegistry();
}

FTopLevelAssetPath FAssetDiscoveryService::GetAssetClassPath(const FString& ClassName) const
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
        {TEXT("StaticMesh"), TEXT("/Script/Engine.StaticMesh")},
        {TEXT("SkeletalMesh"), TEXT("/Script/Engine.SkeletalMesh")},
        {TEXT("Sound"), TEXT("/Script/Engine.SoundBase")},
        {TEXT("SoundWave"), TEXT("/Script/Engine.SoundWave")},
        {TEXT("DataTable"), TEXT("/Script/Engine.DataTable")},
        {TEXT("Curve"), TEXT("/Script/Engine.CurveBase")},
    };
    
    if (const FString* FoundPath = ClassPathMap.Find(ClassName))
    {
        return FTopLevelAssetPath(*FoundPath);
    }
    
    // If not in map, try to construct path assuming it's in Engine
    return FTopLevelAssetPath(FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
}

TResult<TArray<FAssetData>> FAssetDiscoveryService::SearchAssets(const FString& SearchTerm, const FString& AssetType)
{
    IAssetRegistry* AssetRegistry = GetAssetRegistry();
    if (!AssetRegistry)
    {
        return TResult<TArray<FAssetData>>::Error(
            VibeUE::ErrorCodes::INTERNAL_ERROR,
            TEXT("Failed to access Asset Registry")
        );
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
        return TResult<TArray<FAssetData>>::Success(AllAssets);
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
    
    return TResult<TArray<FAssetData>>::Success(MatchingAssets);
}

TResult<TArray<FAssetData>> FAssetDiscoveryService::GetAssetsByType(const FString& AssetType)
{
    if (AssetType.IsEmpty())
    {
        return TResult<TArray<FAssetData>>::Error(
            VibeUE::ErrorCodes::PARAM_EMPTY,
            TEXT("AssetType cannot be empty")
        );
    }
    
    IAssetRegistry* AssetRegistry = GetAssetRegistry();
    if (!AssetRegistry)
    {
        return TResult<TArray<FAssetData>>::Error(
            VibeUE::ErrorCodes::INTERNAL_ERROR,
            TEXT("Failed to access Asset Registry")
        );
    }
    
    TArray<FAssetData> Assets;
    FARFilter Filter;
    
    FTopLevelAssetPath ClassPath = GetAssetClassPath(AssetType);
    if (!ClassPath.IsNull())
    {
        Filter.ClassPaths.Add(ClassPath);
    }
    
    AssetRegistry->GetAssets(Filter, Assets);
    
    return TResult<TArray<FAssetData>>::Success(Assets);
}

TResult<FAssetData> FAssetDiscoveryService::FindAssetByName(const FString& AssetName, const FString& AssetType)
{
    if (AssetName.IsEmpty())
    {
        return TResult<FAssetData>::Error(
            VibeUE::ErrorCodes::PARAM_EMPTY,
            TEXT("AssetName cannot be empty")
        );
    }
    
    IAssetRegistry* AssetRegistry = GetAssetRegistry();
    if (!AssetRegistry)
    {
        return TResult<FAssetData>::Error(
            VibeUE::ErrorCodes::INTERNAL_ERROR,
            TEXT("Failed to access Asset Registry")
        );
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
    
    AssetRegistry->GetAssets(Filter, AllAssets);
    
    // Find exact name match
    for (const FAssetData& Asset : AllAssets)
    {
        if (Asset.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase))
        {
            return TResult<FAssetData>::Success(Asset);
        }
    }
    
    return TResult<FAssetData>::Error(
        VibeUE::ErrorCodes::ASSET_NOT_FOUND,
        FString::Printf(TEXT("Asset not found: %s"), *AssetName)
    );
}

TResult<FAssetData> FAssetDiscoveryService::FindAssetByPath(const FString& AssetPath)
{
    if (AssetPath.IsEmpty())
    {
        return TResult<FAssetData>::Error(
            VibeUE::ErrorCodes::PARAM_EMPTY,
            TEXT("AssetPath cannot be empty")
        );
    }
    
    IAssetRegistry* AssetRegistry = GetAssetRegistry();
    if (!AssetRegistry)
    {
        return TResult<FAssetData>::Error(
            VibeUE::ErrorCodes::INTERNAL_ERROR,
            TEXT("Failed to access Asset Registry")
        );
    }
    
    FAssetData AssetData = AssetRegistry->GetAssetByObjectPath(FSoftObjectPath(AssetPath));
    
    if (AssetData.IsValid())
    {
        return TResult<FAssetData>::Success(AssetData);
    }
    
    return TResult<FAssetData>::Error(
        VibeUE::ErrorCodes::ASSET_NOT_FOUND,
        FString::Printf(TEXT("Asset not found at path: %s"), *AssetPath)
    );
}
