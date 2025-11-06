/**
 * @file WidgetComponentService.cpp
 * @brief Implementation of widget component management functionality
 * 
 * This service provides widget component add/remove operations,
 * extracted from UMGCommands.cpp as part of Phase 4 refactoring.
 */

#include "Services/UMG/WidgetComponentService.h"
#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Kismet2/BlueprintEditorUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetComponent, Log, All);

FWidgetComponentService::FWidgetComponentService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<UWidget*> FWidgetComponentService::AddWidgetComponent(
    UWidgetBlueprint* WidgetBlueprint,
    const FString& WidgetClassName,
    const FString& WidgetName,
    const FString& ParentName,
    bool bIsVariable)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<UWidget*>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(WidgetClassName, TEXT("WidgetClassName"));
    if (ValidationResult.IsError())
    {
        return TResult<UWidget*>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(WidgetName, TEXT("WidgetName"));
    if (ValidationResult.IsError())
    {
        return TResult<UWidget*>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    // Get widget class
    UClass* WidgetClass = GetWidgetClass(WidgetClassName);
    if (!WidgetClass)
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
            FString::Printf(TEXT("Widget class '%s' not found"), *WidgetClassName)
        );
    }

    // Check if name is unique
    if (!IsWidgetNameUnique(WidgetBlueprint, WidgetName))
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::COMPONENT_NAME_EXISTS,
            FString::Printf(TEXT("Widget named '%s' already exists"), *WidgetName)
        );
    }

    // Get or create widget tree
    if (!WidgetBlueprint->WidgetTree)
    {
        WidgetBlueprint->WidgetTree = NewObject<UWidgetTree>(WidgetBlueprint);
    }

    // Create widget
    UWidget* NewWidget = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
    if (!NewWidget)
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::WIDGET_CREATE_FAILED,
            FString::Printf(TEXT("Failed to create widget of type '%s'"), *WidgetClassName)
        );
    }

    // Set as root or add to parent
    if (ParentName.IsEmpty())
    {
        WidgetBlueprint->WidgetTree->RootWidget = NewWidget;
    }
    else
    {
        UWidget* ParentWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentName));
        if (!ParentWidget)
        {
            return TResult<UWidget*>::Error(
                VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
                FString::Printf(TEXT("Parent widget '%s' not found"), *ParentName)
            );
        }

        UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
        if (!ParentPanel)
        {
            return TResult<UWidget*>::Error(
                VibeUE::ErrorCodes::WIDGET_PARENT_INCOMPATIBLE,
                FString::Printf(TEXT("Parent widget '%s' is not a panel widget"), *ParentName)
            );
        }

        ParentPanel->AddChild(NewWidget);
    }

    // Make it a variable if requested
    if (bIsVariable)
    {
        NewWidget->bIsVariable = true;
    }

    WidgetBlueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

    return TResult<UWidget*>::Success(NewWidget);
}

TResult<void> FWidgetComponentService::RemoveWidgetComponent(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    ValidationResult = ValidateNotEmpty(WidgetName, TEXT("WidgetName"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget blueprint has no widget tree")
        );
    }

    UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!Widget)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            FString::Printf(TEXT("Widget '%s' not found"), *WidgetName)
        );
    }

    WidgetBlueprint->WidgetTree->RemoveWidget(Widget);
    WidgetBlueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

    return TResult<void>::Success();
}

TResult<void> FWidgetComponentService::AddChildToPanel(
    UWidgetBlueprint* WidgetBlueprint,
    const FString& ChildName,
    const FString& ParentName)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget blueprint has no widget tree")
        );
    }

    UWidget* ChildWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ChildName));
    if (!ChildWidget)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            FString::Printf(TEXT("Child widget '%s' not found"), *ChildName)
        );
    }

    UWidget* ParentWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentName));
    if (!ParentWidget)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            FString::Printf(TEXT("Parent widget '%s' not found"), *ParentName)
        );
    }

    UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
    if (!ParentPanel)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_PARENT_INCOMPATIBLE,
            FString::Printf(TEXT("Parent widget '%s' is not a panel widget"), *ParentName)
        );
    }

    ParentPanel->AddChild(ChildWidget);
    WidgetBlueprint->Modify();

    return TResult<void>::Success();
}

TResult<UWidget*> FWidgetComponentService::CreateAndAddWidget(
    UWidgetBlueprint* WidgetBlueprint,
    const FString& WidgetClassName,
    const FString& WidgetName,
    const FString& ParentName,
    bool bIsVariable,
    const TMap<FString, FString>& InitialProperties)
{
    // First create the widget
    TResult<UWidget*> CreateResult = AddWidgetComponent(WidgetBlueprint, WidgetClassName, WidgetName, ParentName, bIsVariable);
    if (CreateResult.IsError())
    {
        return CreateResult;
    }

    // TODO: Apply initial properties using WidgetPropertyService
    // This would require dependency injection or service lookup

    return CreateResult;
}

TResult<bool> FWidgetComponentService::ValidateWidgetCreation(
    UWidgetBlueprint* WidgetBlueprint,
    const FString& WidgetClassName,
    const FString& WidgetName,
    const FString& ParentName)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    // Check widget class exists
    UClass* WidgetClass = GetWidgetClass(WidgetClassName);
    if (!WidgetClass)
    {
        return TResult<bool>::Success(false);
    }

    // Check name is unique
    if (!IsWidgetNameUnique(WidgetBlueprint, WidgetName))
    {
        return TResult<bool>::Success(false);
    }

    // Check parent exists if specified
    if (!ParentName.IsEmpty())
    {
        if (!WidgetBlueprint->WidgetTree)
        {
            return TResult<bool>::Success(false);
        }

        UWidget* ParentWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentName));
        if (!ParentWidget || !Cast<UPanelWidget>(ParentWidget))
        {
            return TResult<bool>::Success(false);
        }
    }

    return TResult<bool>::Success(true);
}

TResult<UPanelWidget*> FWidgetComponentService::GetParentPanel(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<UPanelWidget*>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<UPanelWidget*>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget blueprint has no widget tree")
        );
    }

    UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    if (!Widget)
    {
        return TResult<UPanelWidget*>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            FString::Printf(TEXT("Widget '%s' not found"), *WidgetName)
        );
    }

    UPanelSlot* Slot = Widget->Slot;
    if (!Slot)
    {
        return TResult<UPanelWidget*>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget is not in a panel")
        );
    }

    UPanelWidget* ParentPanel = Cast<UPanelWidget>(Slot->Parent);
    if (!ParentPanel)
    {
        return TResult<UPanelWidget*>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget parent is not a panel widget")
        );
    }

    return TResult<UPanelWidget*>::Success(ParentPanel);
}

UClass* FWidgetComponentService::GetWidgetClass(const FString& WidgetClassName)
{
    UClass* WidgetClass = FindObject<UClass>(ANY_PACKAGE, *WidgetClassName);
    if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
    {
        return nullptr;
    }
    return WidgetClass;
}

bool FWidgetComponentService::IsWidgetNameUnique(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
{
    if (!WidgetBlueprint->WidgetTree)
    {
        return true;
    }

    UWidget* ExistingWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
    return ExistingWidget == nullptr;
}
