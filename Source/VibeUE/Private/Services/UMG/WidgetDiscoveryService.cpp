#include "Services/UMG/WidgetDiscoveryService.h"
#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/GarbageCollection.h"

FWidgetDiscoveryService::FWidgetDiscoveryService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

bool FWidgetDiscoveryService::IsInSerializationContext() const
{
    return IsGarbageCollecting() || GIsSavingPackage || IsLoading();
}

IAssetRegistry& FWidgetDiscoveryService::GetAssetRegistry()
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    return AssetRegistryModule.Get();
}

TResult<UWidgetBlueprint*> FWidgetDiscoveryService::FindWidget(const FString& WidgetName)
{
    // Check if we're in a serialization context to prevent crashes
    if (IsInSerializationContext())
    {
        return TResult<UWidgetBlueprint*>::Failure(
            ErrorCodes::InvalidOperation,
            TEXT("Cannot find widget during serialization context")
        );
    }

    if (WidgetName.IsEmpty())
    {
        return TResult<UWidgetBlueprint*>::Failure(
            ErrorCodes::InvalidArgument,
            TEXT("Widget name cannot be empty")
        );
    }

    UE_LOG(LogTemp, Log, TEXT("WidgetDiscoveryService: Searching for widget '%s'"), *WidgetName);
    
    // PRIORITY 1: First try direct path loading for exact matches (most reliable)
    UWidgetBlueprint* DirectLoad = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(WidgetName));
    if (DirectLoad)
    {
        UE_LOG(LogTemp, Log, TEXT("WidgetDiscoveryService: Found widget via direct load"));
        return TResult<UWidgetBlueprint*>::Success(DirectLoad);
    }

    // PRIORITY 2: If it's a package path without object name, try append .AssetName
    if (WidgetName.StartsWith(TEXT("/Game")) && !WidgetName.Contains(TEXT(".")))
    {
        FString AssetName;
        int32 SlashIdx;
        if (WidgetName.FindLastChar('/', SlashIdx))
        {
            AssetName = WidgetName.Mid(SlashIdx + 1);
            if (!AssetName.IsEmpty())
            {
                const FString ObjectPath = WidgetName + TEXT(".") + AssetName;
                UE_LOG(LogTemp, Verbose, TEXT("WidgetDiscoveryService: Trying object path '%s'"), *ObjectPath);
                DirectLoad = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(ObjectPath));
                if (DirectLoad)
                {
                    UE_LOG(LogTemp, Log, TEXT("WidgetDiscoveryService: Found widget via constructed object path"));
                    return TResult<UWidgetBlueprint*>::Success(DirectLoad);
                }
            }
        }
    }
    
    // PRIORITY 3: Use Asset Registry for search with priority-based matching
    IAssetRegistry& AssetRegistry = GetAssetRegistry();
    
    // Create filter to find all Widget Blueprints
    FARFilter Filter;
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add("/Game"); // Search recursively from /Game
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    
    UE_LOG(LogTemp, Verbose, TEXT("WidgetDiscoveryService: Found %d widget blueprints in asset registry"), AssetDataList.Num());
    
    // IMPROVED MATCHING LOGIC: Use priority-based matching to avoid wrong widget selection
    UWidgetBlueprint* BestMatch = nullptr;
    int32 BestMatchPriority = 0;
    
    for (const FAssetData& AssetData : AssetDataList)
    {
        FString AssetName = AssetData.AssetName.ToString();
        FString PackagePath = AssetData.PackageName.ToString();
        const FString ObjectPath = AssetData.GetObjectPathString();
        
        int32 MatchPriority = 0;
        
        // PRIORITY 1 (10): Exact asset name match (case sensitive)
        if (AssetName.Equals(WidgetName, ESearchCase::CaseSensitive))
        {
            MatchPriority = 10;
        }
        // PRIORITY 2 (9): Exact asset name match (case insensitive)
        else if (AssetName.Equals(WidgetName, ESearchCase::IgnoreCase))
        {
            MatchPriority = 9;
        }
        // PRIORITY 3 (8): Full object path match (case sensitive)
        else if (ObjectPath.Equals(WidgetName, ESearchCase::CaseSensitive))
        {
            MatchPriority = 8;
        }
        // PRIORITY 4 (7): Full package path match (case sensitive)
        else if (PackagePath.Equals(WidgetName, ESearchCase::CaseSensitive))
        {
            MatchPriority = 7;
        }
        // PRIORITY 5 (6): Full object path match (case insensitive)
        else if (ObjectPath.Equals(WidgetName, ESearchCase::IgnoreCase))
        {
            MatchPriority = 6;
        }
        // PRIORITY 6 (5): Full package path match (case insensitive)
        else if (PackagePath.Equals(WidgetName, ESearchCase::IgnoreCase))
        {
            MatchPriority = 5;
        }
        // PRIORITY 7 (3): Asset name starts with search term (exact prefix match)
        else if (AssetName.StartsWith(WidgetName, ESearchCase::IgnoreCase) && AssetName.Len() > WidgetName.Len())
        {
            MatchPriority = 3;
        }
        // PRIORITY 8 (2): Asset name contains search term (but avoid partial matches that could be wrong)
        else if (AssetName.Contains(WidgetName, ESearchCase::IgnoreCase) && 
                 WidgetName.Len() >= 3) // Only allow contains matching for terms 3+ chars to avoid false positives
        {
            MatchPriority = 2;
        }
        // PRIORITY 9 (1): Package path contains search term (lowest priority, most error-prone)
        else if (PackagePath.Contains(WidgetName, ESearchCase::IgnoreCase) && 
                 WidgetName.Len() >= 4) // Only allow contains matching for terms 4+ chars
        {
            MatchPriority = 1;
        }
        
        // Only consider this match if it's better than what we have
        if (MatchPriority > BestMatchPriority)
        {
            UWidgetBlueprint* CandidateWidget = Cast<UWidgetBlueprint>(AssetData.GetAsset());
            if (CandidateWidget)
            {
                UE_LOG(LogTemp, Verbose, TEXT("WidgetDiscoveryService: Found better match '%s' with priority %d"), *AssetName, MatchPriority);
                BestMatch = CandidateWidget;
                BestMatchPriority = MatchPriority;
                
                // If we found an exact match, we can stop searching
                if (MatchPriority >= 9)
                {
                    break;
                }
            }
        }
    }
    
    if (BestMatch)
    {
        UE_LOG(LogTemp, Log, TEXT("WidgetDiscoveryService: Returning best match '%s' with priority %d"), 
            *BestMatch->GetName(), BestMatchPriority);
        return TResult<UWidgetBlueprint*>::Success(BestMatch);
    }
    
    return TResult<UWidgetBlueprint*>::Failure(
        ErrorCodes::AssetNotFound,
        FString::Printf(TEXT("Widget blueprint '%s' not found"), *WidgetName)
    );
}

TResult<UWidgetBlueprint*> FWidgetDiscoveryService::LoadWidget(const FString& WidgetPath)
{
    if (IsInSerializationContext())
    {
        return TResult<UWidgetBlueprint*>::Failure(
            ErrorCodes::InvalidOperation,
            TEXT("Cannot load widget during serialization context")
        );
    }

    if (WidgetPath.IsEmpty())
    {
        return TResult<UWidgetBlueprint*>::Failure(
            ErrorCodes::InvalidArgument,
            TEXT("Widget path cannot be empty")
        );
    }

    UWidgetBlueprint* Widget = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(WidgetPath));
    if (!Widget)
    {
        return TResult<UWidgetBlueprint*>::Failure(
            ErrorCodes::AssetNotFound,
            FString::Printf(TEXT("Failed to load widget from path '%s'"), *WidgetPath)
        );
    }

    return TResult<UWidgetBlueprint*>::Success(Widget);
}

TResult<TArray<FAssetData>> FWidgetDiscoveryService::SearchWidgets(const FString& SearchTerm, int32 MaxResults)
{
    IAssetRegistry& AssetRegistry = GetAssetRegistry();
    
    // Create filter for Widget Blueprints
    FARFilter Filter;
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add("/Game");
    
    TArray<FAssetData> AllWidgets;
    AssetRegistry.GetAssets(Filter, AllWidgets);
    
    if (SearchTerm.IsEmpty())
    {
        // Return all widgets, limited by MaxResults
        if (AllWidgets.Num() > MaxResults)
        {
            AllWidgets.SetNum(MaxResults);
        }
        return TResult<TArray<FAssetData>>::Success(AllWidgets);
    }
    
    // Filter by search term
    TArray<FAssetData> MatchingWidgets;
    for (const FAssetData& AssetData : AllWidgets)
    {
        const FString AssetName = AssetData.AssetName.ToString();
        const FString PackagePath = AssetData.PackageName.ToString();
        
        if (AssetName.Contains(SearchTerm, ESearchCase::IgnoreCase) ||
            PackagePath.Contains(SearchTerm, ESearchCase::IgnoreCase))
        {
            MatchingWidgets.Add(AssetData);
            
            if (MatchingWidgets.Num() >= MaxResults)
            {
                break;
            }
        }
    }
    
    return TResult<TArray<FAssetData>>::Success(MatchingWidgets);
}

TResult<TArray<FAssetData>> FWidgetDiscoveryService::GetAllWidgets()
{
    return SearchWidgets(TEXT(""), 1000); // Large default limit
}

TResult<bool> FWidgetDiscoveryService::WidgetExists(const FString& WidgetName)
{
    auto Result = FindWidget(WidgetName);
    return TResult<bool>::Success(Result.IsSuccess());
}

TResult<bool> FWidgetDiscoveryService::IsValidWidget(const FString& WidgetName)
{
    auto Result = FindWidget(WidgetName);
    if (!Result.IsSuccess())
    {
        return TResult<bool>::Success(false);
    }
    
    // Additional validation: check if widget is valid
    UWidgetBlueprint* Widget = Result.GetValue();
    bool bIsValid = Widget != nullptr && Widget->IsValidLowLevel() && !Widget->IsPendingKill();
    
    return TResult<bool>::Success(bIsValid);
}

TResult<TArray<FString>> FWidgetDiscoveryService::GetAvailableWidgetTypes()
{
    TArray<FString> WidgetTypes = {
        TEXT("Border"),
        TEXT("Button"),
        TEXT("CheckBox"),
        TEXT("CircularThrobber"),
        TEXT("ComboBoxString"),
        TEXT("EditableText"),
        TEXT("EditableTextBox"),
        TEXT("Image"),
        TEXT("ProgressBar"),
        TEXT("ScrollBar"),
        TEXT("Slider"),
        TEXT("Spacer"),
        TEXT("Spinner"),
        TEXT("TextBlock"),
        TEXT("Throbber"),
        TEXT("NamedSlot"),
        TEXT("RichTextBlock"),
        TEXT("InputKeySelector"),
        TEXT("AnalogSlider"),
        TEXT("CommonButton"),
        TEXT("CommonTextBlock")
    };
    
    return TResult<TArray<FString>>::Success(WidgetTypes);
}

TResult<TArray<FString>> FWidgetDiscoveryService::GetAvailablePanelTypes()
{
    TArray<FString> PanelTypes = {
        TEXT("CanvasPanel"),
        TEXT("VerticalBox"),
        TEXT("HorizontalBox"),
        TEXT("GridPanel"),
        TEXT("UniformGridPanel"),
        TEXT("WrapBox"),
        TEXT("ScrollBox"),
        TEXT("Overlay"),
        TEXT("SizeBox"),
        TEXT("ScaleBox"),
        TEXT("WidgetSwitcher"),
        TEXT("InvalidationBox")
    };
    
    return TResult<TArray<FString>>::Success(PanelTypes);
}

TResult<TArray<FString>> FWidgetDiscoveryService::GetCommonWidgets()
{
    TArray<FString> CommonWidgets = {
        TEXT("Button"),
        TEXT("TextBlock"),
        TEXT("Image"),
        TEXT("VerticalBox"),
        TEXT("HorizontalBox"),
        TEXT("CanvasPanel"),
        TEXT("Border"),
        TEXT("EditableTextBox"),
        TEXT("ProgressBar"),
        TEXT("Slider")
    };
    
    return TResult<TArray<FString>>::Success(CommonWidgets);
}
