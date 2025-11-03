// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/UMG/WidgetComponentService.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Core/ErrorCodes.h"

FWidgetComponentService::FWidgetComponentService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<UWidget*> FWidgetComponentService::AddComponent(
    UWidgetBlueprint* Widget,
    const FString& ComponentType,
    const FString& ComponentName,
    const FString& ParentName)
{
    // Validate inputs
    auto ValidationResult = ValidateWidget(Widget);
    if (ValidationResult.IsError())
    {
        return TResult<UWidget*>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    auto NameValidation = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
    if (NameValidation.IsError())
    {
        return TResult<UWidget*>::Error(NameValidation.GetErrorCode(), NameValidation.GetErrorMessage());
    }

    auto TypeValidation = ValidateNotEmpty(ComponentType, TEXT("ComponentType"));
    if (TypeValidation.IsError())
    {
        return TResult<UWidget*>::Error(TypeValidation.GetErrorCode(), TypeValidation.GetErrorMessage());
    }

    // Check if component already exists
    if (Widget->WidgetTree->FindWidget(FName(*ComponentName)))
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::COMPONENT_NAME_EXISTS,
            FString::Printf(TEXT("Component '%s' already exists"), *ComponentName));
    }

    // Get widget class
    UClass* WidgetClass = GetWidgetClass(ComponentType);
    if (!WidgetClass)
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::COMPONENT_TYPE_INVALID,
            FString::Printf(TEXT("Unknown widget type '%s'"), *ComponentType));
    }

    // Create the widget
    UWidget* NewWidget = Widget->WidgetTree->ConstructWidget<UWidget>(WidgetClass, *ComponentName);
    if (!NewWidget)
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::COMPONENT_CREATE_FAILED,
            FString::Printf(TEXT("Failed to create widget of type '%s'"), *ComponentType));
    }

    // Find parent and add widget
    UPanelWidget* ParentPanel = nullptr;
    if (ParentName.IsEmpty())
    {
        // Add to root
        ParentPanel = Cast<UPanelWidget>(Widget->WidgetTree->RootWidget);
    }
    else
    {
        UWidget* ParentWidget = FindComponent(Widget->WidgetTree, ParentName);
        if (!ParentWidget)
        {
            return TResult<UWidget*>::Error(
                VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND,
                FString::Printf(TEXT("Parent component '%s' not found"), *ParentName));
        }
        ParentPanel = Cast<UPanelWidget>(ParentWidget);
    }

    if (!ParentPanel)
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::WIDGET_PARENT_INCOMPATIBLE,
            TEXT("Parent is not a panel widget that can contain children"));
    }

    // Add to parent
    ParentPanel->AddChild(NewWidget);

    // Mark blueprint as modified and compile
    Widget->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(Widget);

    LogInfo(FString::Printf(TEXT("Added component '%s' of type '%s'"), *ComponentName, *ComponentType));
    return TResult<UWidget*>::Success(NewWidget);
}

TResult<void> FWidgetComponentService::RemoveComponent(
    UWidgetBlueprint* Widget,
    const FString& ComponentName,
    bool bRemoveChildren)
{
    // Validate inputs
    auto ValidationResult = ValidateWidget(Widget);
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    auto NameValidation = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
    if (NameValidation.IsError())
    {
        return NameValidation;
    }

    // Find the component
    UWidget* TargetComponent = FindComponent(Widget->WidgetTree, ComponentName);
    if (!TargetComponent)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::COMPONENT_NOT_FOUND,
            FString::Printf(TEXT("Component '%s' not found"), *ComponentName));
    }

    // Collect children
    TArray<UWidget*> AllChildren;
    CollectChildren(TargetComponent, AllChildren);

    // Handle children based on flag
    if (!bRemoveChildren && AllChildren.Num() > 0)
    {
        // Reparent children to root
        UPanelWidget* RootPanel = Cast<UPanelWidget>(Widget->WidgetTree->RootWidget);
        if (RootPanel)
        {
            for (UWidget* Child : AllChildren)
            {
                if (UWidget* CurrentParent = Child->GetParent())
                {
                    if (UPanelWidget* CurrentPanel = Cast<UPanelWidget>(CurrentParent))
                    {
                        CurrentPanel->RemoveChild(Child);
                    }
                }
                RootPanel->AddChild(Child);
            }
        }
    }

    // Remove from parent
    UWidget* ParentWidget = TargetComponent->GetParent();
    if (ParentWidget)
    {
        if (UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget))
        {
            ParentPanel->RemoveChild(TargetComponent);
        }
        else
        {
            return TResult<void>::Error(
                VibeUE::ErrorCodes::WIDGET_PARENT_INCOMPATIBLE,
                TEXT("Parent is not a panel widget"));
        }
    }
    else if (Widget->WidgetTree->RootWidget == TargetComponent)
    {
        // Removing root widget
        Widget->WidgetTree->RootWidget = nullptr;
    }

    // Mark blueprint as modified and compile
    Widget->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(Widget);

    LogInfo(FString::Printf(TEXT("Removed component '%s'"), *ComponentName));
    return TResult<void>::Success();
}

TResult<TArray<FWidgetComponentInfo>> FWidgetComponentService::ListComponents(UWidgetBlueprint* Widget)
{
    // Validate inputs
    auto ValidationResult = ValidateWidget(Widget);
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FWidgetComponentInfo>>::Error(
            ValidationResult.GetErrorCode(), 
            ValidationResult.GetErrorMessage());
    }

    TArray<FWidgetComponentInfo> Components;
    TArray<UWidget*> AllWidgets;
    Widget->WidgetTree->GetAllWidgets(AllWidgets);

    for (UWidget* WidgetComponent : AllWidgets)
    {
        if (WidgetComponent)
        {
            FWidgetComponentInfo Info;
            Info.Name = WidgetComponent->GetName();
            Info.Type = WidgetComponent->GetClass()->GetName();
            Info.bIsVariable = WidgetComponent->bIsVariable;

            // Get parent name
            UWidget* ParentWidget = WidgetComponent->GetParent();
            Info.ParentName = ParentWidget ? ParentWidget->GetName() : TEXT("");

            // Get children
            if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(WidgetComponent))
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

            Components.Add(Info);
        }
    }

    return TResult<TArray<FWidgetComponentInfo>>::Success(Components);
}

TResult<void> FWidgetComponentService::SetParent(
    UWidgetBlueprint* Widget,
    const FString& ComponentName,
    const FString& NewParentName)
{
    // Validate inputs
    auto ValidationResult = ValidateWidget(Widget);
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    auto NameValidation = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
    if (NameValidation.IsError())
    {
        return NameValidation;
    }

    auto ParentValidation = ValidateNotEmpty(NewParentName, TEXT("NewParentName"));
    if (ParentValidation.IsError())
    {
        return ParentValidation;
    }

    // Find component
    UWidget* Component = FindComponent(Widget->WidgetTree, ComponentName);
    if (!Component)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::COMPONENT_NOT_FOUND,
            FString::Printf(TEXT("Component '%s' not found"), *ComponentName));
    }

    // Find new parent
    UWidget* NewParentWidget = FindComponent(Widget->WidgetTree, NewParentName);
    if (!NewParentWidget)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND,
            FString::Printf(TEXT("Parent component '%s' not found"), *NewParentName));
    }

    UPanelWidget* NewParentPanel = Cast<UPanelWidget>(NewParentWidget);
    if (!NewParentPanel)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_PARENT_INCOMPATIBLE,
            TEXT("New parent is not a panel widget"));
    }

    // Remove from current parent
    UWidget* CurrentParent = Component->GetParent();
    if (CurrentParent)
    {
        if (UPanelWidget* CurrentPanel = Cast<UPanelWidget>(CurrentParent))
        {
            CurrentPanel->RemoveChild(Component);
        }
    }

    // Add to new parent
    NewParentPanel->AddChild(Component);

    // Mark blueprint as modified and compile
    Widget->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(Widget);

    LogInfo(FString::Printf(TEXT("Reparented '%s' to '%s'"), *ComponentName, *NewParentName));
    return TResult<void>::Success();
}

TResult<FString> FWidgetComponentService::GetParent(
    UWidgetBlueprint* Widget,
    const FString& ComponentName)
{
    // Validate inputs
    auto ValidationResult = ValidateWidget(Widget);
    if (ValidationResult.IsError())
    {
        return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    auto NameValidation = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
    if (NameValidation.IsError())
    {
        return TResult<FString>::Error(NameValidation.GetErrorCode(), NameValidation.GetErrorMessage());
    }

    // Find component
    UWidget* Component = FindComponent(Widget->WidgetTree, ComponentName);
    if (!Component)
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::COMPONENT_NOT_FOUND,
            FString::Printf(TEXT("Component '%s' not found"), *ComponentName));
    }

    // Get parent
    UWidget* ParentWidget = Component->GetParent();
    FString ParentName = ParentWidget ? ParentWidget->GetName() : TEXT("");

    return TResult<FString>::Success(ParentName);
}

TResult<TArray<FString>> FWidgetComponentService::GetChildren(
    UWidgetBlueprint* Widget,
    const FString& ComponentName)
{
    // Validate inputs
    auto ValidationResult = ValidateWidget(Widget);
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FString>>::Error(
            ValidationResult.GetErrorCode(),
            ValidationResult.GetErrorMessage());
    }

    auto NameValidation = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
    if (NameValidation.IsError())
    {
        return TResult<TArray<FString>>::Error(
            NameValidation.GetErrorCode(),
            NameValidation.GetErrorMessage());
    }

    // Find component
    UWidget* Component = FindComponent(Widget->WidgetTree, ComponentName);
    if (!Component)
    {
        return TResult<TArray<FString>>::Error(
            VibeUE::ErrorCodes::COMPONENT_NOT_FOUND,
            FString::Printf(TEXT("Component '%s' not found"), *ComponentName));
    }

    TArray<FString> Children;
    if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Component))
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

TResult<FWidgetComponentInfo> FWidgetComponentService::GetComponentInfo(
    UWidgetBlueprint* Widget,
    const FString& ComponentName)
{
    // Validate inputs
    auto ValidationResult = ValidateWidget(Widget);
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetComponentInfo>::Error(
            ValidationResult.GetErrorCode(),
            ValidationResult.GetErrorMessage());
    }

    auto NameValidation = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
    if (NameValidation.IsError())
    {
        return TResult<FWidgetComponentInfo>::Error(
            NameValidation.GetErrorCode(),
            NameValidation.GetErrorMessage());
    }

    // Find component
    UWidget* Component = FindComponent(Widget->WidgetTree, ComponentName);
    if (!Component)
    {
        return TResult<FWidgetComponentInfo>::Error(
            VibeUE::ErrorCodes::COMPONENT_NOT_FOUND,
            FString::Printf(TEXT("Component '%s' not found"), *ComponentName));
    }

    FWidgetComponentInfo Info;
    Info.Name = Component->GetName();
    Info.Type = Component->GetClass()->GetName();
    Info.bIsVariable = Component->bIsVariable;

    // Get parent
    UWidget* ParentWidget = Component->GetParent();
    Info.ParentName = ParentWidget ? ParentWidget->GetName() : TEXT("");

    // Get children
    if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Component))
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

    return TResult<FWidgetComponentInfo>::Success(Info);
}

TResult<bool> FWidgetComponentService::ComponentExists(
    UWidgetBlueprint* Widget,
    const FString& ComponentName)
{
    // Validate inputs
    auto ValidationResult = ValidateWidget(Widget);
    if (ValidationResult.IsError())
    {
        return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    auto NameValidation = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
    if (NameValidation.IsError())
    {
        return TResult<bool>::Error(NameValidation.GetErrorCode(), NameValidation.GetErrorMessage());
    }

    bool bExists = FindComponent(Widget->WidgetTree, ComponentName) != nullptr;
    return TResult<bool>::Success(bExists);
}

// Private helper methods

TResult<void> FWidgetComponentService::ValidateWidget(UWidgetBlueprint* Widget) const
{
    if (!Widget)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_BLUEPRINT_NOT_FOUND,
            TEXT("Widget blueprint is null"));
    }

    if (!Widget->WidgetTree)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::WIDGET_BLUEPRINT_NOT_FOUND,
            TEXT("Widget tree is null"));
    }

    return TResult<void>::Success();
}

UWidget* FWidgetComponentService::FindComponent(UWidgetTree* WidgetTree, const FString& ComponentName) const
{
    if (!WidgetTree)
    {
        return nullptr;
    }

    return WidgetTree->FindWidget(FName(*ComponentName));
}

void FWidgetComponentService::CollectChildren(UWidget* Widget, TArray<UWidget*>& OutChildren) const
{
    if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
    {
        for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
        {
            UWidget* Child = Panel->GetChildAt(i);
            if (Child)
            {
                OutChildren.Add(Child);
                CollectChildren(Child, OutChildren); // Recursive
            }
        }
    }
}

UClass* FWidgetComponentService::GetWidgetClass(const FString& ComponentType) const
{
    // Map common widget type names to classes
    static const TMap<FString, UClass*> WidgetTypeMap = {
        {TEXT("TextBlock"), UTextBlock::StaticClass()},
        {TEXT("Button"), UButton::StaticClass()},
        {TEXT("Image"), UImage::StaticClass()},
        {TEXT("CanvasPanel"), UCanvasPanel::StaticClass()},
        {TEXT("VerticalBox"), UVerticalBox::StaticClass()},
        {TEXT("HorizontalBox"), UHorizontalBox::StaticClass()},
        {TEXT("Overlay"), UOverlay::StaticClass()},
        {TEXT("ScrollBox"), UScrollBox::StaticClass()},
        {TEXT("Border"), UBorder::StaticClass()},
        {TEXT("SizeBox"), USizeBox::StaticClass()}
    };

    if (const UClass* const* FoundClass = WidgetTypeMap.Find(ComponentType))
    {
        return *FoundClass;
    }

    // Try to find class by name
    UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("U%s"), *ComponentType));
    return FoundClass;
}
