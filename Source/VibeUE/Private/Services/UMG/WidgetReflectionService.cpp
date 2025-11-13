// Copyright VibeUE 2025

/**
 * @file WidgetReflectionService.cpp
 * @brief Implementation of widget reflection and discovery functionality
 * 
 * This service provides widget class discovery using UClass reflection,
 * extracted from UMGReflectionCommands.cpp as part of Phase 4 refactoring.
 */

#include "Services/UMG/WidgetReflectionService.h"
#include "Core/ErrorCodes.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/Overlay.h"
#include "Components/GridPanel.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetReflection, Log, All);

FWidgetReflectionService::FWidgetReflectionService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<TArray<FString>> FWidgetReflectionService::GetAvailableWidgetTypes()
{
    TArray<FString> WidgetTypes;
    
    TArray<UClass*> Classes = DiscoverWidgetClasses(true, true);
    for (UClass* Class : Classes)
    {
        if (Class)
        {
            WidgetTypes.Add(Class->GetName());
        }
    }
    
    return TResult<TArray<FString>>::Success(WidgetTypes);
}

TResult<TArray<FString>> FWidgetReflectionService::GetWidgetCategories()
{
    TArray<FString> Categories;
    
    // Standard UMG categories
    Categories.Add(TEXT("Panel"));
    Categories.Add(TEXT("Common"));
    Categories.Add(TEXT("Input"));
    Categories.Add(TEXT("Primitive"));
    Categories.Add(TEXT("Misc"));
    
    return TResult<TArray<FString>>::Success(Categories);
}

TResult<TArray<FWidgetClassInfo>> FWidgetReflectionService::GetAvailableWidgetClasses(bool bIncludeEngine, bool bIncludeCustom)
{
    TArray<FWidgetClassInfo> ClassInfos;
    
    TArray<UClass*> Classes = DiscoverWidgetClasses(bIncludeEngine, bIncludeCustom);
    for (UClass* Class : Classes)
    {
        if (Class)
        {
            FWidgetClassInfo Info;
            Info.ClassName = Class->GetName();
            Info.ClassPath = Class->GetPathName();
            Info.Category = GetCategoryForClass(Class);
            Info.bSupportsChildren = DoesClassSupportChildren(Class);
            Info.MaxChildren = GetMaxChildrenForClass(Class);
            Info.bIsPanel = Class->IsChildOf(UPanelWidget::StaticClass());
            Info.bIsEngineWidget = Class->GetOutermost()->GetName().StartsWith(TEXT("/Script/UMG"));
            Info.bIsCustomWidget = !Info.bIsEngineWidget;
            
            ClassInfos.Add(Info);
        }
    }
    
    return TResult<TArray<FWidgetClassInfo>>::Success(ClassInfos);
}

TResult<TArray<FString>> FWidgetReflectionService::GetWidgetsByCategory(const FString& Category)
{
    TArray<FString> Widgets;
    
    TArray<UClass*> Classes = DiscoverWidgetClasses(true, true);
    for (UClass* Class : Classes)
    {
        if (Class && GetCategoryForClass(Class).Equals(Category, ESearchCase::IgnoreCase))
        {
            Widgets.Add(Class->GetName());
        }
    }
    
    return TResult<TArray<FString>>::Success(Widgets);
}

TResult<TArray<FString>> FWidgetReflectionService::GetPanelWidgets()
{
    TArray<FString> PanelWidgets;
    
    TArray<UClass*> Classes = DiscoverWidgetClasses(true, true);
    for (UClass* Class : Classes)
    {
        if (Class && Class->IsChildOf(UPanelWidget::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
        {
            PanelWidgets.Add(Class->GetName());
        }
    }
    
    return TResult<TArray<FString>>::Success(PanelWidgets);
}

TResult<TArray<FString>> FWidgetReflectionService::GetCommonWidgets()
{
    TArray<FString> CommonWidgets;
    
    // Commonly used widgets
    CommonWidgets.Add(TEXT("Button"));
    CommonWidgets.Add(TEXT("TextBlock"));
    CommonWidgets.Add(TEXT("Image"));
    CommonWidgets.Add(TEXT("EditableText"));
    CommonWidgets.Add(TEXT("CheckBox"));
    CommonWidgets.Add(TEXT("Slider"));
    CommonWidgets.Add(TEXT("ProgressBar"));
    
    return TResult<TArray<FString>>::Success(CommonWidgets);
}

TResult<FWidgetClassInfo> FWidgetReflectionService::GetWidgetClassInfo(const FString& WidgetClassName)
{
    UClass* WidgetClass = FindFirstObjectSafe<UClass>(*WidgetClassName);
    if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
    {
        return TResult<FWidgetClassInfo>::Error(
            VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
            FString::Printf(TEXT("Widget class '%s' not found"), *WidgetClassName)
        );
    }
    
    FWidgetClassInfo Info;
    Info.ClassName = WidgetClass->GetName();
    Info.ClassPath = WidgetClass->GetPathName();
    Info.Category = GetCategoryForClass(WidgetClass);
    Info.bSupportsChildren = DoesClassSupportChildren(WidgetClass);
    Info.MaxChildren = GetMaxChildrenForClass(WidgetClass);
    Info.bIsPanel = WidgetClass->IsChildOf(UPanelWidget::StaticClass());
    Info.bIsEngineWidget = WidgetClass->GetOutermost()->GetName().StartsWith(TEXT("/Script/UMG"));
    Info.bIsCustomWidget = !Info.bIsEngineWidget;
    
    return TResult<FWidgetClassInfo>::Success(Info);
}

TResult<bool> FWidgetReflectionService::SupportsChildren(const FString& WidgetClassName)
{
    UClass* WidgetClass = FindFirstObjectSafe<UClass>(*WidgetClassName);
    if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
    {
        return TResult<bool>::Error(
            VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
            FString::Printf(TEXT("Widget class '%s' not found"), *WidgetClassName)
        );
    }
    
    bool bSupports = DoesClassSupportChildren(WidgetClass);
    return TResult<bool>::Success(bSupports);
}

TResult<FWidgetCompatibilityInfo> FWidgetReflectionService::CheckCompatibility(const FString& ParentClassName, const FString& ChildClassName)
{
    UClass* ParentClass = FindFirstObjectSafe<UClass>(*ParentClassName);
    UClass* ChildClass = FindFirstObjectSafe<UClass>(*ChildClassName);
    
    FWidgetCompatibilityInfo Info;
    Info.ParentClass = ParentClassName;
    Info.ChildClass = ChildClassName;
    
    if (!ParentClass || !ParentClass->IsChildOf(UWidget::StaticClass()))
    {
        Info.bIsCompatible = false;
        Info.IncompatibilityReason = FString::Printf(TEXT("Parent class '%s' not found"), *ParentClassName);
        return TResult<FWidgetCompatibilityInfo>::Success(Info);
    }
    
    if (!ChildClass || !ChildClass->IsChildOf(UWidget::StaticClass()))
    {
        Info.bIsCompatible = false;
        Info.IncompatibilityReason = FString::Printf(TEXT("Child class '%s' not found"), *ChildClassName);
        return TResult<FWidgetCompatibilityInfo>::Success(Info);
    }
    
    // Check if parent supports children
    if (!ParentClass->IsChildOf(UPanelWidget::StaticClass()))
    {
        Info.bIsCompatible = false;
        Info.IncompatibilityReason = FString::Printf(TEXT("Parent '%s' is not a panel widget and cannot have children"), *ParentClassName);
        return TResult<FWidgetCompatibilityInfo>::Success(Info);
    }
    
    // All panel widgets accept all widget children by default
    Info.bIsCompatible = true;
    return TResult<FWidgetCompatibilityInfo>::Success(Info);
}

TResult<int32> FWidgetReflectionService::GetMaxChildrenCount(const FString& WidgetClassName)
{
    UClass* WidgetClass = FindFirstObjectSafe<UClass>(*WidgetClassName);
    if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
    {
        return TResult<int32>::Error(
            VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
            FString::Printf(TEXT("Widget class '%s' not found"), *WidgetClassName)
        );
    }
    
    int32 MaxChildren = GetMaxChildrenForClass(WidgetClass);
    return TResult<int32>::Success(MaxChildren);
}

TResult<FString> FWidgetReflectionService::GetWidgetCategory(const FString& WidgetClassName)
{
    UClass* WidgetClass = FindFirstObjectSafe<UClass>(*WidgetClassName);
    if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
            FString::Printf(TEXT("Widget class '%s' not found"), *WidgetClassName)
        );
    }
    
    FString Category = GetCategoryForClass(WidgetClass);
    return TResult<FString>::Success(Category);
}

TArray<UClass*> FWidgetReflectionService::DiscoverWidgetClasses(bool bIncludeEngine, bool bIncludeCustom)
{
    TArray<UClass*> WidgetClasses;
    
    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;
        if (!Class || !Class->IsChildOf(UWidget::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
        {
            continue;
        }
        
        bool bIsEngineWidget = Class->GetOutermost()->GetName().StartsWith(TEXT("/Script/UMG"));
        
        if ((bIncludeEngine && bIsEngineWidget) || (bIncludeCustom && !bIsEngineWidget))
        {
            WidgetClasses.Add(Class);
        }
    }
    
    return WidgetClasses;
}

FString FWidgetReflectionService::GetCategoryForClass(UClass* WidgetClass)
{
    if (!WidgetClass)
    {
        return TEXT("Misc");
    }
    
    // Panel widgets
    if (WidgetClass->IsChildOf(UPanelWidget::StaticClass()))
    {
        return TEXT("Panel");
    }
    
    // Common widgets
    if (WidgetClass == UButton::StaticClass() ||
        WidgetClass == UTextBlock::StaticClass() ||
        WidgetClass == UImage::StaticClass())
    {
        return TEXT("Common");
    }
    
    // Input widgets
    FString ClassName = WidgetClass->GetName();
    if (ClassName.Contains(TEXT("Editable")) || 
        ClassName.Contains(TEXT("CheckBox")) ||
        ClassName.Contains(TEXT("Slider")))
    {
        return TEXT("Input");
    }
    
    return TEXT("Misc");
}

bool FWidgetReflectionService::DoesClassSupportChildren(UClass* WidgetClass)
{
    if (!WidgetClass)
    {
        return false;
    }
    
    return WidgetClass->IsChildOf(UPanelWidget::StaticClass());
}

int32 FWidgetReflectionService::GetMaxChildrenForClass(UClass* WidgetClass)
{
    if (!WidgetClass || !DoesClassSupportChildren(WidgetClass))
    {
        return 0;
    }
    
    // Most panel widgets support unlimited children
    return -1;
}
