/**
 * @file BlueprintDiscoveryService.cpp
 * @brief Implementation of Blueprint discovery and loading functionality
 * 
 * This service provides focused blueprint discovery operations extracted from the
 * monolithic BlueprintCommands.cpp file as part of the Phase 2 refactoring effort.
 * 
 * The implementation uses the Unreal Engine Asset Registry for efficient blueprint
 * discovery and supports multiple search strategies including direct path loading,
 * default path conventions, and recursive name-based searches.
 * 
 * @see FBlueprintDiscoveryService
 * @see CPP_REFACTORING_DESIGN.md
 */

#include "Services/Blueprint/BlueprintDiscoveryService.h"
#include "Core/ErrorCodes.h"
#include "Engine/Blueprint.h"
#include "WidgetBlueprint.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"

FBlueprintDiscoveryService::FBlueprintDiscoveryService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<UBlueprint*> FBlueprintDiscoveryService::FindBlueprint(const FString& BlueprintName)
{
    if (BlueprintName.IsEmpty())
    {
        return TResult<UBlueprint*>::Error(
            ErrorCodes::PARAM_INVALID,
            TEXT("Blueprint name cannot be empty")
        );
    }

    FString NormalizedName = BlueprintName.TrimStartAndEnd();
    UBlueprint* LoadedBlueprint = nullptr;

    auto ExtractAssetName = [](const FString& Path) -> FString
    {
        int32 SlashIndex = INDEX_NONE;
        if (Path.FindLastChar(TEXT('/'), SlashIndex))
        {
            return Path.Mid(SlashIndex + 1);
        }
        return Path;
    };

    auto TryLoadByPath = [&LoadedBlueprint](const FString& AssetPath)
    {
        if (!LoadedBlueprint)
        {
            LoadedBlueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(AssetPath));
        }
        if (!LoadedBlueprint)
        {
            LoadedBlueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
        }
    };

    // Handle full or partial asset paths
    if (NormalizedName.StartsWith(TEXT("/")))
    {
        FString AssetPath = NormalizedName;
        if (!AssetPath.Contains(TEXT(".")))
        {
            const FString AssetName = ExtractAssetName(AssetPath);
            if (!AssetName.IsEmpty())
            {
                AssetPath += TEXT(".") + AssetName;
            }
        }

        TryLoadByPath(AssetPath);
        if (LoadedBlueprint)
        {
            return TResult<UBlueprint*>::Success(LoadedBlueprint);
        }
    }
    else
    {
        TryLoadByPath(NormalizedName);
        if (LoadedBlueprint)
        {
            return TResult<UBlueprint*>::Success(LoadedBlueprint);
        }

        // Try default path under /Game/Blueprints/
        const FString DefaultPackage = FString::Printf(TEXT("/Game/Blueprints/%s"), *NormalizedName);
        FString DefaultAssetPath = DefaultPackage;
        if (!DefaultAssetPath.Contains(TEXT(".")))
        {
            const FString AssetName = ExtractAssetName(DefaultPackage);
            DefaultAssetPath += TEXT(".") + AssetName;
        }

        TryLoadByPath(DefaultAssetPath);
        if (LoadedBlueprint)
        {
            return TResult<UBlueprint*>::Success(LoadedBlueprint);
        }
    }

    // Use Asset Registry for recursive search
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add("/Game");
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    
    for (const FAssetData& AssetData : AssetDataList)
    {
        FString AssetName = AssetData.AssetName.ToString();
        if (AssetName.Equals(BlueprintName, ESearchCase::IgnoreCase))
        {
            UBlueprint* FoundBlueprint = Cast<UBlueprint>(AssetData.GetAsset());
            if (FoundBlueprint)
            {
                return TResult<UBlueprint*>::Success(FoundBlueprint);
            }
        }
    }
    
    return TResult<UBlueprint*>::Error(
        ErrorCodes::BLUEPRINT_NOT_FOUND,
        FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName)
    );
}

TResult<UBlueprint*> FBlueprintDiscoveryService::LoadBlueprint(const FString& BlueprintPath)
{
    if (BlueprintPath.IsEmpty())
    {
        return TResult<UBlueprint*>::Error(
            ErrorCodes::PARAM_INVALID,
            TEXT("Blueprint path cannot be empty")
        );
    }

    UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    if (!Blueprint)
    {
        Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    }

    if (!Blueprint)
    {
        return TResult<UBlueprint*>::Error(
            ErrorCodes::BLUEPRINT_LOAD_FAILED,
            FString::Printf(TEXT("Failed to load blueprint from path: %s"), *BlueprintPath)
        );
    }

    return TResult<UBlueprint*>::Success(Blueprint);
}

TResult<TArray<FBlueprintInfo>> FBlueprintDiscoveryService::SearchBlueprints(const FString& SearchTerm, int32 MaxResults)
{
    if (SearchTerm.IsEmpty())
    {
        return TResult<TArray<FBlueprintInfo>>::Error(
            ErrorCodes::PARAM_INVALID,
            TEXT("Search term cannot be empty")
        );
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add("/Game");
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    
    TArray<FBlueprintInfo> Results;
    FString LowerSearchTerm = SearchTerm.ToLower();
    
    for (const FAssetData& AssetData : AssetDataList)
    {
        if (Results.Num() >= MaxResults)
        {
            break;
        }

        FString AssetName = AssetData.AssetName.ToString().ToLower();
        if (AssetName.Contains(LowerSearchTerm))
        {
            UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
            if (Blueprint)
            {
                TResult<FBlueprintInfo> InfoResult = GetBlueprintInfo(Blueprint);
                if (InfoResult.IsSuccess())
                {
                    Results.Add(InfoResult.GetValue());
                }
            }
        }
    }
    
    return TResult<TArray<FBlueprintInfo>>::Success(Results);
}

TResult<TArray<FString>> FBlueprintDiscoveryService::ListAllBlueprints(const FString& BasePath)
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add(*BasePath);
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    
    TArray<FString> Results;
    for (const FAssetData& AssetData : AssetDataList)
    {
        Results.Add(AssetData.GetObjectPathString());
    }
    
    return TResult<TArray<FString>>::Success(Results);
}

TResult<FBlueprintInfo> FBlueprintDiscoveryService::GetBlueprintInfo(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return TResult<FBlueprintInfo>::Error(
            ErrorCodes::PARAM_INVALID,
            TEXT("Blueprint pointer is null")
        );
    }

    FBlueprintInfo Info;
    Info.Name = Blueprint->GetName();
    Info.Path = Blueprint->GetPathName();
    Info.PackagePath = Blueprint->GetPackage() ? Blueprint->GetPackage()->GetPathName() : TEXT("");
    Info.ParentClass = Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("Unknown");
    Info.BlueprintType = Blueprint->GetClass()->GetName();
    Info.bIsWidgetBlueprint = Blueprint->IsA<UWidgetBlueprint>();

    return TResult<FBlueprintInfo>::Success(Info);
}

TResult<bool> FBlueprintDiscoveryService::BlueprintExists(const FString& BlueprintName)
{
    if (BlueprintName.IsEmpty())
    {
        return TResult<bool>::Error(
            ErrorCodes::PARAM_INVALID,
            TEXT("Blueprint name cannot be empty")
        );
    }

    TResult<UBlueprint*> Result = FindBlueprint(BlueprintName);
    return TResult<bool>::Success(Result.IsSuccess());
}
