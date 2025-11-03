#include "Services/UMG/WidgetReflectionService.h"
#include "Services/Blueprint/BlueprintReflectionService.h"
#include "Core/ErrorCodes.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/ScrollBox.h"
#include "Components/GridPanel.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/CheckBox.h"
#include "Components/Spacer.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    // Common events for all widgets
    const TArray<FString> CommonWidgetEvents = { TEXT("OnVisibilityChanged") };
}

FWidgetReflectionService::FWidgetReflectionService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
    , bCatalogsInitialized(false)
{
}

void FWidgetReflectionService::InitializeWidgetCatalogs()
{
    if (bCatalogsInitialized) return;
    
    PanelWidgetTypes = { TEXT("CanvasPanel"), TEXT("VerticalBox"), TEXT("HorizontalBox"), 
        TEXT("Overlay"), TEXT("ScrollBox"), TEXT("GridPanel"), TEXT("UniformGridPanel"), 
        TEXT("WidgetSwitcher"), TEXT("SizeBox"), TEXT("Border") };
    
    CommonWidgetTypes = { TEXT("Button"), TEXT("TextBlock"), TEXT("Image"), 
        TEXT("EditableTextBox"), TEXT("Slider"), TEXT("ProgressBar"), TEXT("CheckBox") };
    
    AllWidgetTypes = { TEXT("TextBlock"), TEXT("Button"), TEXT("EditableText"), 
        TEXT("EditableTextBox"), TEXT("RichTextBlock"), TEXT("CheckBox"), TEXT("Slider"), 
        TEXT("ProgressBar"), TEXT("Image"), TEXT("Spacer"), TEXT("Border"), TEXT("SizeBox"), 
        TEXT("CanvasPanel"), TEXT("VerticalBox"), TEXT("HorizontalBox"), TEXT("Overlay"), 
        TEXT("ScrollBox"), TEXT("GridPanel"), TEXT("UniformGridPanel"), TEXT("WidgetSwitcher") };
    
    WidgetTypeToClassPath = {
        {TEXT("TextBlock"), TEXT("/Script/UMG.TextBlock")},
        {TEXT("Button"), TEXT("/Script/UMG.Button")},
        {TEXT("EditableText"), TEXT("/Script/UMG.EditableText")},
        {TEXT("EditableTextBox"), TEXT("/Script/UMG.EditableTextBox")},
        {TEXT("RichTextBlock"), TEXT("/Script/UMG.RichTextBlock")},
        {TEXT("CheckBox"), TEXT("/Script/UMG.CheckBox")},
        {TEXT("Slider"), TEXT("/Script/UMG.Slider")},
        {TEXT("ProgressBar"), TEXT("/Script/UMG.ProgressBar")},
        {TEXT("Image"), TEXT("/Script/UMG.Image")},
        {TEXT("Spacer"), TEXT("/Script/UMG.Spacer")},
        {TEXT("Border"), TEXT("/Script/UMG.Border")},
        {TEXT("SizeBox"), TEXT("/Script/UMG.SizeBox")},
        {TEXT("CanvasPanel"), TEXT("/Script/UMG.CanvasPanel")},
        {TEXT("VerticalBox"), TEXT("/Script/UMG.VerticalBox")},
        {TEXT("HorizontalBox"), TEXT("/Script/UMG.HorizontalBox")},
        {TEXT("Overlay"), TEXT("/Script/UMG.Overlay")},
        {TEXT("ScrollBox"), TEXT("/Script/UMG.ScrollBox")},
        {TEXT("GridPanel"), TEXT("/Script/UMG.GridPanel")},
        {TEXT("UniformGridPanel"), TEXT("/Script/UMG.UniformGridPanel")},
        {TEXT("WidgetSwitcher"), TEXT("/Script/UMG.WidgetSwitcher")}
    };
    
    // Category mappings (Panel widgets get "Panel" automatically in GetWidgetTypeInfo)
    WidgetTypeToCategory = {
        {TEXT("EditableText"), TEXT("Input")}, {TEXT("EditableTextBox"), TEXT("Input")},
        {TEXT("Slider"), TEXT("Input")}, {TEXT("CheckBox"), TEXT("Input")},
        {TEXT("Button"), TEXT("Input")},
        {TEXT("TextBlock"), TEXT("Display")}, {TEXT("Image"), TEXT("Display")},
        {TEXT("ProgressBar"), TEXT("Display")}, {TEXT("RichTextBlock"), TEXT("Display")},
        {TEXT("Spacer"), TEXT("Layout")}, {TEXT("Border"), TEXT("Layout")},
        {TEXT("SizeBox"), TEXT("Layout")}
    };
    
    // Event mappings
    WidgetTypeToEvents = {
        {TEXT("Button"), {TEXT("OnClicked"), TEXT("OnPressed"), TEXT("OnReleased"), TEXT("OnHovered"), TEXT("OnUnhovered")}},
        {TEXT("CheckBox"), {TEXT("OnCheckStateChanged")}},
        {TEXT("Slider"), {TEXT("OnValueChanged"), TEXT("OnMouseCaptureBegin"), TEXT("OnMouseCaptureEnd")}},
        {TEXT("EditableText"), {TEXT("OnTextChanged"), TEXT("OnTextCommitted")}},
        {TEXT("EditableTextBox"), {TEXT("OnTextChanged"), TEXT("OnTextCommitted")}}
    };
    
    bCatalogsInitialized = true;
}

TResult<TArray<FString>> FWidgetReflectionService::GetAvailableWidgetTypes(const FString& Category)
{
    InitializeWidgetCatalogs();
    
    if (Category.IsEmpty())
    {
        return TResult<TArray<FString>>::Success(AllWidgetTypes);
    }
    
    if (Category.Equals(TEXT("Panel"), ESearchCase::IgnoreCase))
    {
        return TResult<TArray<FString>>::Success(PanelWidgetTypes);
    }
    else if (Category.Equals(TEXT("Common"), ESearchCase::IgnoreCase))
    {
        return TResult<TArray<FString>>::Success(CommonWidgetTypes);
    }
    else
    {
        return TResult<TArray<FString>>::Error(
            VibeUE::ErrorCodes::PARAM_INVALID,
            FString::Printf(TEXT("Unknown widget category: %s"), *Category)
        );
    }
}

TResult<TArray<FString>> FWidgetReflectionService::GetWidgetCategories()
{
    return TResult<TArray<FString>>::Success(TArray<FString>{ 
        TEXT("Panel"), TEXT("Common"), TEXT("Input"), TEXT("Display"), TEXT("Layout") 
    });
}

TResult<TArray<FString>> FWidgetReflectionService::GetPanelWidgets()
{
    InitializeWidgetCatalogs();
    return TResult<TArray<FString>>::Success(PanelWidgetTypes);
}

TResult<TArray<FString>> FWidgetReflectionService::GetCommonWidgets()
{
    InitializeWidgetCatalogs();
    return TResult<TArray<FString>>::Success(CommonWidgetTypes);
}

TResult<FWidgetTypeInfo> FWidgetReflectionService::GetWidgetTypeInfo(const FString& WidgetType)
{
    InitializeWidgetCatalogs();
    
    auto ValidationResult = IsValidWidgetType(WidgetType);
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetTypeInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }
    
    FWidgetTypeInfo Info;
    Info.TypeName = WidgetType;
    Info.ClassPath = WidgetTypeToClassPath.Contains(WidgetType) ? WidgetTypeToClassPath[WidgetType] : TEXT("");
    Info.bIsPanelWidget = PanelWidgetTypes.Contains(WidgetType);
    Info.bIsCommonWidget = CommonWidgetTypes.Contains(WidgetType);
    
    // Assign category from lookup table or default
    if (Info.bIsPanelWidget)
    {
        Info.Category = TEXT("Panel");
    }
    else if (WidgetTypeToCategory.Contains(WidgetType))
    {
        Info.Category = WidgetTypeToCategory[WidgetType];
    }
    else
    {
        Info.Category = TEXT("Other");
    }
    
    return TResult<FWidgetTypeInfo>::Success(Info);
}

TResult<TArray<FPropertyInfo>> FWidgetReflectionService::GetWidgetTypeProperties(const FString& WidgetType)
{
    auto ClassResult = ResolveWidgetClass(WidgetType);
    if (ClassResult.IsError())
    {
        return TResult<TArray<FPropertyInfo>>::Error(ClassResult.GetErrorCode(), ClassResult.GetErrorMessage());
    }
    
    UClass* WidgetClass = ClassResult.GetValue();
    if (!WidgetClass)
    {
        return TResult<TArray<FPropertyInfo>>::Error(VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
            FString::Printf(TEXT("Failed to resolve widget class for type: %s"), *WidgetType));
    }
    
    TArray<FPropertyInfo> Properties;
    for (TFieldIterator<FProperty> PropIt(WidgetClass); PropIt; ++PropIt)
    {
        FProperty* Prop = *PropIt;
        if (!Prop || !Prop->HasAnyPropertyFlags(CPF_Edit)) continue;
        
        FPropertyInfo PropInfo;
        PropInfo.PropertyName = Prop->GetName();
        PropInfo.PropertyType = Prop->GetCPPType();
        PropInfo.bIsEditable = true;
        
        const FString* CategoryPtr = Prop->FindMetaData(TEXT("Category"));
        if (CategoryPtr) PropInfo.Category = *CategoryPtr;
        
        Properties.Add(PropInfo);
    }
    
    return TResult<TArray<FPropertyInfo>>::Success(Properties);
}

TResult<TArray<FString>> FWidgetReflectionService::GetWidgetTypeEvents(const FString& WidgetType)
{
    InitializeWidgetCatalogs();
    
    auto ClassResult = ResolveWidgetClass(WidgetType);
    if (ClassResult.IsError())
    {
        return TResult<TArray<FString>>::Error(ClassResult.GetErrorCode(), ClassResult.GetErrorMessage());
    }
    
    TArray<FString> Events;
    
    // Get widget-specific events from lookup table
    if (WidgetTypeToEvents.Contains(WidgetType))
    {
        Events = WidgetTypeToEvents[WidgetType];
    }
    
    // Add common events for all widgets
    Events.Append(CommonWidgetEvents);
    
    return TResult<TArray<FString>>::Success(Events);
}

TResult<bool> FWidgetReflectionService::IsValidWidgetType(const FString& WidgetType)
{
    InitializeWidgetCatalogs();
    
    if (WidgetType.IsEmpty())
    {
        return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_EMPTY, TEXT("Widget type cannot be empty"));
    }
    
    return TResult<bool>::Success(AllWidgetTypes.Contains(WidgetType));
}

TResult<bool> FWidgetReflectionService::IsPanelWidget(const FString& WidgetType)
{
    InitializeWidgetCatalogs();
    
    auto ValidationResult = IsValidWidgetType(WidgetType);
    if (ValidationResult.IsError())
    {
        return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }
    
    return TResult<bool>::Success(PanelWidgetTypes.Contains(WidgetType));
}

TResult<bool> FWidgetReflectionService::CanContainChildren(const FString& WidgetType)
{
    return IsPanelWidget(WidgetType);
}

TResult<UClass*> FWidgetReflectionService::ResolveWidgetClass(const FString& WidgetType)
{
    InitializeWidgetCatalogs();
    
    auto ValidationResult = IsValidWidgetType(WidgetType);
    if (ValidationResult.IsError())
    {
        return TResult<UClass*>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }
    
    if (!WidgetTypeToClassPath.Contains(WidgetType))
    {
        return TResult<UClass*>::Error(VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
            FString::Printf(TEXT("No class path mapping for widget type: %s"), *WidgetType));
    }
    
    UClass* WidgetClass = FindObject<UClass>(nullptr, *WidgetTypeToClassPath[WidgetType]);
    if (!WidgetClass)
    {
        return TResult<UClass*>::Error(VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
            FString::Printf(TEXT("Failed to find widget class at path: %s"), *WidgetTypeToClassPath[WidgetType]));
    }
    
    return TResult<UClass*>::Success(WidgetClass);
}

TResult<FString> FWidgetReflectionService::GetWidgetTypePath(const FString& WidgetType)
{
    InitializeWidgetCatalogs();
    
    auto ValidationResult = IsValidWidgetType(WidgetType);
    if (ValidationResult.IsError())
    {
        return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }
    
    if (!WidgetTypeToClassPath.Contains(WidgetType))
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
            FString::Printf(TEXT("No class path mapping for widget type: %s"), *WidgetType));
    }
    
    return TResult<FString>::Success(WidgetTypeToClassPath[WidgetType]);
}
