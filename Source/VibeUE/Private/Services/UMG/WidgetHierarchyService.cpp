/**
 * @file WidgetHierarchyService.cpp
 * @brief Implementation of widget hierarchy navigation and validation
 * 
 * This service provides widget tree navigation and validation,
 * extracted from UMGCommands.cpp as part of Phase 4 refactoring.
 */

#include "Services/UMG/WidgetHierarchyService.h"
#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetHierarchy, Log, All);

FWidgetHierarchyService::FWidgetHierarchyService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<FWidgetHierarchy> FWidgetHierarchyService::GetWidgetHierarchy(UWidgetBlueprint* WidgetBlueprint)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetHierarchy>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    FWidgetHierarchy Hierarchy;

    if (!WidgetBlueprint->WidgetTree || !WidgetBlueprint->WidgetTree->RootWidget)
    {
        return TResult<FWidgetHierarchy>::Success(Hierarchy);
    }

    BuildHierarchyRecursive(WidgetBlueprint->WidgetTree->RootWidget, Hierarchy);
    Hierarchy.TotalCount = Hierarchy.Components.Num();

    return TResult<FWidgetHierarchy>::Success(Hierarchy);
}

TResult<TArray<FWidgetInfo>> FWidgetHierarchyService::ListWidgetComponents(UWidgetBlueprint* WidgetBlueprint)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FWidgetInfo>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TArray<FWidgetInfo> Components;

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<TArray<FWidgetInfo>>::Success(Components);
    }

    TArray<UWidget*> AllWidgets;
    WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);

    for (UWidget* Widget : AllWidgets)
    {
        if (Widget)
        {
            TResult<FWidgetInfo> InfoResult = GetWidgetInfo(Widget);
            if (InfoResult.IsSuccess())
            {
                Components.Add(InfoResult.GetValue());
            }
        }
    }

    return TResult<TArray<FWidgetInfo>>::Success(Components);
}

TResult<FWidgetInfo> FWidgetHierarchyService::GetWidgetInfo(UWidget* Widget)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    FWidgetInfo Info;
    Info.Name = Widget->GetName();
    Info.Type = Widget->GetClass()->GetName();
    Info.bIsVariable = Widget->bIsVariable;

    // Get parent
    if (Widget->Slot && Widget->Slot->Parent)
    {
        Info.ParentName = Widget->Slot->Parent->GetName();
    }

    // Get children if panel widget
    UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget);
    if (PanelWidget)
    {
        for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
        {
            UWidget* Child = PanelWidget->GetChildAt(i);
            if (Child)
            {
                Info.Children.Add(Child->GetName());
            }
        }
    }

    return TResult<FWidgetInfo>::Success(Info);
}

TResult<TArray<FString>> FWidgetHierarchyService::GetWidgetChildren(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FString>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<TArray<FString>>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget blueprint has no widget tree")
        );
    }

    UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!Widget)
    {
        return TResult<TArray<FString>>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            FString::Printf(TEXT("Widget '%s' not found"), *WidgetName)
        );
    }

    TArray<FString> Children;

    UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget);
    if (PanelWidget)
    {
        for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
        {
            UWidget* Child = PanelWidget->GetChildAt(i);
            if (Child)
            {
                Children.Add(Child->GetName());
            }
        }
    }

    return TResult<TArray<FString>>::Success(Children);
}

TResult<FString> FWidgetHierarchyService::GetWidgetParent(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget blueprint has no widget tree")
        );
    }

    UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!Widget)
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            FString::Printf(TEXT("Widget '%s' not found"), *WidgetName)
        );
    }

    FString ParentName;
    if (Widget->Slot && Widget->Slot->Parent)
    {
        ParentName = Widget->Slot->Parent->GetName();
    }

    return TResult<FString>::Success(ParentName);
}

TResult<TArray<FString>> FWidgetHierarchyService::ValidateWidgetHierarchy(UWidgetBlueprint* WidgetBlueprint)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FString>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TArray<FString> ValidationErrors;

    if (!WidgetBlueprint->WidgetTree)
    {
        ValidationErrors.Add(TEXT("Widget blueprint has no widget tree"));
        return TResult<TArray<FString>>::Success(ValidationErrors);
    }

    if (!WidgetBlueprint->WidgetTree->RootWidget)
    {
        ValidationErrors.Add(TEXT("Widget tree has no root widget"));
    }

    // Additional validation could be added here
    // For example: checking for circular references, duplicate names, etc.

    return TResult<TArray<FString>>::Success(ValidationErrors);
}

TResult<UWidget*> FWidgetHierarchyService::GetRootWidget(UWidgetBlueprint* WidgetBlueprint)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
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

    UWidget* RootWidget = WidgetBlueprint->WidgetTree->RootWidget;
    if (!RootWidget)
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget tree has no root widget")
        );
    }

    return TResult<UWidget*>::Success(RootWidget);
}

TResult<int32> FWidgetHierarchyService::GetWidgetDepth(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<int32>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<int32>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget blueprint has no widget tree")
        );
    }

    UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!Widget)
    {
        return TResult<int32>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            FString::Printf(TEXT("Widget '%s' not found"), *WidgetName)
        );
    }

    int32 Depth = 0;
    UWidget* CurrentWidget = Widget;
    
    while (CurrentWidget && CurrentWidget->Slot && CurrentWidget->Slot->Parent)
    {
        Depth++;
        CurrentWidget = Cast<UWidget>(CurrentWidget->Slot->Parent);
    }

    return TResult<int32>::Success(Depth);
}

void FWidgetHierarchyService::BuildHierarchyRecursive(UWidget* Widget, FWidgetHierarchy& OutHierarchy)
{
    if (!Widget)
    {
        return;
    }

    TResult<FWidgetInfo> InfoResult = GetWidgetInfo(Widget);
    if (InfoResult.IsSuccess())
    {
        OutHierarchy.Components.Add(InfoResult.GetValue());
    }

    UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget);
    if (PanelWidget)
    {
        for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
        {
            UWidget* Child = PanelWidget->GetChildAt(i);
            if (Child)
            {
                BuildHierarchyRecursive(Child, OutHierarchy);
            }
        }
    }
}

void FWidgetHierarchyService::GetAllWidgetsRecursive(UWidget* Widget, TArray<UWidget*>& OutWidgets)
{
    if (!Widget)
    {
        return;
    }

    OutWidgets.Add(Widget);

    UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget);
    if (PanelWidget)
    {
        for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
        {
            UWidget* Child = PanelWidget->GetChildAt(i);
            if (Child)
            {
                GetAllWidgetsRecursive(Child, OutWidgets);
            }
        }
    }
}
