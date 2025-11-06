/**
 * @file WidgetDiscoveryService.cpp
 * @brief Implementation of widget blueprint discovery functionality
 * 
 * This service provides widget blueprint discovery and loading,
 * extracted from UMGCommands.cpp as part of Phase 4 refactoring.
 */

#include "Services/UMG/WidgetDiscoveryService.h"
#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetDiscovery, Log, All);

// Helper function to extract asset name from path
static FString ExtractAssetNameFromPath(const FString& Path)
{
    int32 SlashIndex = INDEX_NONE;
    if (Path.FindLastChar(TEXT('/'), SlashIndex))
    {
        return Path.Mid(SlashIndex + 1);
    }
    return Path;
}

// Helper function to try loading a widget blueprint by path
static UWidgetBlueprint* TryLoadWidgetBlueprintByPath(const FString& AssetPath)
{
    // CRITICAL: Cannot load assets during garbage collection or serialization
    if (IsGarbageCollecting() || GIsSavingPackage || FUObjectThreadContext::Get().IsRoutingPostLoad)
    {
        UE_LOG(LogWidgetDiscovery, Warning, TEXT("Cannot load Widget Blueprint '%s' during serialization/GC"), *AssetPath);
        return nullptr;
    }

    UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!WidgetBP)
    {
        WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
    }
    return WidgetBP;
}

FWidgetDiscoveryService::FWidgetDiscoveryService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<UWidgetBlueprint*> FWidgetDiscoveryService::FindWidgetBlueprint(const FString& WidgetBlueprintName)
{
    if (WidgetBlueprintName.IsEmpty())
    {
        return TResult<UWidgetBlueprint*>::Error(
            VibeUE::ErrorCodes::PARAM_INVALID,
            TEXT("Widget blueprint name cannot be empty")
        );
    }

    FString NormalizedName = WidgetBlueprintName.TrimStartAndEnd();

    // Handle full asset paths
    if (NormalizedName.StartsWith(TEXT("/")))
    {
        FString AssetPath = NormalizedName;
        if (!AssetPath.Contains(TEXT(".")))
        {
            const FString AssetName = ExtractAssetNameFromPath(AssetPath);
            if (!AssetName.IsEmpty())
            {
                AssetPath += TEXT(".") + AssetName;
            }
        }

        UWidgetBlueprint* WidgetBP = TryLoadWidgetBlueprintByPath(AssetPath);
        if (WidgetBP)
        {
            return TResult<UWidgetBlueprint*>::Success(WidgetBP);
        }
    }
    else
    {
        // Try default path under /Game/UI/
        const FString DefaultPackage = FString::Printf(TEXT("/Game/UI/%s"), *NormalizedName);
        FString DefaultAssetPath = DefaultPackage;
        if (!DefaultAssetPath.Contains(TEXT(".")))
        {
            const FString AssetName = ExtractAssetNameFromPath(DefaultPackage);
            DefaultAssetPath += TEXT(".") + AssetName;
        }

        UWidgetBlueprint* WidgetBP = TryLoadWidgetBlueprintByPath(DefaultAssetPath);
        if (WidgetBP)
        {
            return TResult<UWidgetBlueprint*>::Success(WidgetBP);
        }
    }

    // Use Asset Registry for recursive search by name
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    FARFilter Filter;
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add("/Game");
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    
    for (const FAssetData& AssetData : AssetDataList)
    {
        FString AssetName = AssetData.AssetName.ToString();
        if (AssetName.Equals(WidgetBlueprintName, ESearchCase::IgnoreCase))
        {
            UWidgetBlueprint* FoundWidgetBP = Cast<UWidgetBlueprint>(AssetData.GetAsset());
            if (FoundWidgetBP)
            {
                return TResult<UWidgetBlueprint*>::Success(FoundWidgetBP);
            }
        }
    }
    
    return TResult<UWidgetBlueprint*>::Error(
        VibeUE::ErrorCodes::WIDGET_BLUEPRINT_NOT_FOUND,
        FString::Printf(TEXT("Widget blueprint '%s' not found"), *WidgetBlueprintName)
    );
}

TResult<UWidgetBlueprint*> FWidgetDiscoveryService::LoadWidgetBlueprint(const FString& WidgetBlueprintPath)
{
    if (WidgetBlueprintPath.IsEmpty())
    {
        return TResult<UWidgetBlueprint*>::Error(
            VibeUE::ErrorCodes::PARAM_INVALID,
            TEXT("Widget blueprint path cannot be empty")
        );
    }

    UWidgetBlueprint* WidgetBP = TryLoadWidgetBlueprintByPath(WidgetBlueprintPath);
    if (!WidgetBP)
    {
        return TResult<UWidgetBlueprint*>::Error(
            VibeUE::ErrorCodes::WIDGET_BLUEPRINT_NOT_FOUND,
            FString::Printf(TEXT("Failed to load widget blueprint from '%s'"), *WidgetBlueprintPath)
        );
    }

    return TResult<UWidgetBlueprint*>::Success(WidgetBP);
}

TResult<TArray<FWidgetBlueprintInfo>> FWidgetDiscoveryService::SearchWidgetBlueprints(const FString& SearchTerm, int32 MaxResults)
{
    TArray<FWidgetBlueprintInfo> Results;

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    FARFilter Filter;
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add("/Game");
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    
    for (const FAssetData& AssetData : AssetDataList)
    {
        if (Results.Num() >= MaxResults)
        {
            break;
        }

        FString AssetName = AssetData.AssetName.ToString();
        if (SearchTerm.IsEmpty() || AssetName.Contains(SearchTerm, ESearchCase::IgnoreCase))
        {
            FWidgetBlueprintInfo Info;
            Info.Name = AssetName;
            Info.Path = AssetData.GetObjectPathString();
            Info.PackagePath = AssetData.PackagePath.ToString();
            
            Results.Add(Info);
        }
    }

    return TResult<TArray<FWidgetBlueprintInfo>>::Success(Results);
}

TResult<TArray<FString>> FWidgetDiscoveryService::ListAllWidgetBlueprints(const FString& BasePath)
{
    TArray<FString> WidgetBlueprintPaths;

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    FARFilter Filter;
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add(*BasePath);
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    
    for (const FAssetData& AssetData : AssetDataList)
    {
        WidgetBlueprintPaths.Add(AssetData.GetObjectPathString());
    }

    return TResult<TArray<FString>>::Success(WidgetBlueprintPaths);
}

TResult<FWidgetBlueprintInfo> FWidgetDiscoveryService::GetWidgetBlueprintInfo(UWidgetBlueprint* WidgetBlueprint)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetBlueprintInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    FWidgetBlueprintInfo Info;
    Info.Name = WidgetBlueprint->GetName();
    Info.Path = WidgetBlueprint->GetPathName();
    Info.PackagePath = WidgetBlueprint->GetPackage()->GetName();

    if (WidgetBlueprint->ParentClass)
    {
        Info.ParentClass = WidgetBlueprint->ParentClass->GetName();
    }

    if (WidgetBlueprint->WidgetTree && WidgetBlueprint->WidgetTree->RootWidget)
    {
        Info.RootWidget = WidgetBlueprint->WidgetTree->RootWidget->GetName();
        
        // Count widgets in tree
        TArray<UWidget*> AllWidgets;
        WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
        Info.WidgetCount = AllWidgets.Num();
    }

    return TResult<FWidgetBlueprintInfo>::Success(Info);
}

TResult<bool> FWidgetDiscoveryService::WidgetBlueprintExists(const FString& WidgetBlueprintName)
{
    if (WidgetBlueprintName.IsEmpty())
    {
        return TResult<bool>::Error(
            VibeUE::ErrorCodes::PARAM_INVALID,
            TEXT("Widget blueprint name cannot be empty")
        );
    }

    TResult<UWidgetBlueprint*> Result = FindWidgetBlueprint(WidgetBlueprintName);
    return TResult<bool>::Success(Result.IsSuccess());
}

TResult<UWidget*> FWidgetDiscoveryService::FindWidgetByName(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<UWidget*>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(WidgetName, TEXT("WidgetName"));
    if (ValidationResult.IsError())
    {
        return TResult<UWidget*>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget blueprint has no widget tree")
        );
    }

    UWidget* FoundWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!FoundWidget)
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            FString::Printf(TEXT("Widget '%s' not found in blueprint"), *WidgetName)
        );
    }

    return TResult<UWidget*>::Success(FoundWidget);
}
