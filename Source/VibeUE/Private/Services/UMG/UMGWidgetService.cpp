// Copyright Kevin Buckley 2025 All Rights Reserved.

/**
 * @file UMGWidgetService.cpp
 * @brief Implementation of widget component management functionality
 */

#include "Services/UMG/UMGWidgetService.h"

#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Components/SizeBoxSlot.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetComponent, Log, All);

FUMGWidgetService::FUMGWidgetService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<UWidget*> FUMGWidgetService::AddWidgetComponent(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetClassName, const FString& WidgetName, const FString& ParentName, bool bIsVariable)
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

    UClass* WidgetClass = GetWidgetClass(WidgetClassName);
    if (!WidgetClass)
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::COMPONENT_TYPE_INVALID,
            FString::Printf(TEXT("Widget class '%s' not found"), *WidgetClassName)
        );
    }

    if (!IsWidgetNameUnique(WidgetBlueprint, WidgetName))
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::COMPONENT_NAME_EXISTS,
            FString::Printf(TEXT("Widget named '%s' already exists"), *WidgetName)
        );
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        WidgetBlueprint->WidgetTree = NewObject<UWidgetTree>(WidgetBlueprint);
    }

    UWidget* NewWidget = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
    if (!NewWidget)
    {
        return TResult<UWidget*>::Error(
            VibeUE::ErrorCodes::COMPONENT_ADD_FAILED,
            FString::Printf(TEXT("Failed to create widget of type '%s'"), *WidgetClassName)
        );
    }

    if (ParentName.IsEmpty())
    {
        // If there's no root yet, this becomes the root
        if (!WidgetBlueprint->WidgetTree->RootWidget)
        {
            WidgetBlueprint->WidgetTree->RootWidget = NewWidget;
        }
        // If root exists and is a panel, add to it
        else if (UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->RootWidget))
        {
            RootPanel->AddChild(NewWidget);
        }
        // If root exists but is not a panel, we need to wrap both in a CanvasPanel
        else
        {
            UWidget* OldRoot = WidgetBlueprint->WidgetTree->RootWidget;
            
            // Create a new CanvasPanel to be the root
            UCanvasPanel* NewRootPanel = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), FName(TEXT("RootCanvas")));
            if (NewRootPanel)
            {
                // Set as new root
                WidgetBlueprint->WidgetTree->RootWidget = NewRootPanel;
                // Add old root and new widget as children
                NewRootPanel->AddChild(OldRoot);
                NewRootPanel->AddChild(NewWidget);
            }
            else
            {
                // Fallback: just replace root (old behavior, but log warning)
                UE_LOG(LogWidgetComponent, Warning, TEXT("Could not create CanvasPanel wrapper, replacing root widget"));
                WidgetBlueprint->WidgetTree->RootWidget = NewWidget;
            }
        }
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
                VibeUE::ErrorCodes::COMPONENT_TYPE_INCOMPATIBLE,
                FString::Printf(TEXT("Parent widget '%s' is not a panel widget"), *ParentName)
            );
        }

        ParentPanel->AddChild(NewWidget);
    }

    if (bIsVariable)
    {
        NewWidget->bIsVariable = true;
    }

    WidgetBlueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

    return TResult<UWidget*>::Success(NewWidget);
}

TResult<void> FUMGWidgetService::RemoveWidgetComponent(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
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

TResult<FWidgetAddChildResult> FUMGWidgetService::AddChildToPanel(UWidgetBlueprint* WidgetBlueprint, const FWidgetAddChildRequest& Request)
{
    auto BlueprintValidation = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (BlueprintValidation.IsError())
    {
        return TResult<FWidgetAddChildResult>::Error(BlueprintValidation.GetErrorCode(), BlueprintValidation.GetErrorMessage());
    }

    UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
    if (!WidgetTree)
    {
        return TResult<FWidgetAddChildResult>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("WidgetTree not found in widget blueprint"));
    }

    UWidget* ChildWidget = WidgetTree->FindWidget(FName(*Request.ChildName));
    if (!ChildWidget)
    {
        return TResult<FWidgetAddChildResult>::Error(VibeUE::ErrorCodes::WIDGET_NOT_FOUND, FString::Printf(TEXT("Child widget '%s' not found"), *Request.ChildName));
    }

    auto ParentResult = ResolveParentPanel(WidgetBlueprint, Request.ParentName, Request.ParentType);
    if (ParentResult.IsError())
    {
        return TResult<FWidgetAddChildResult>::Error(ParentResult.GetErrorCode(), ParentResult.GetErrorMessage());
    }

    UPanelWidget* ParentPanel = ParentResult.GetValue();
    if (!ParentPanel)
    {
        return TResult<FWidgetAddChildResult>::Error(VibeUE::ErrorCodes::COMPONENT_NOT_FOUND, TEXT("Parent panel not found or could not be created"));
    }

    const bool bAlreadyInParent = (ChildWidget->GetParent() == ParentPanel);
    bool bStructureChanged = false;
    bool bReparented = false;

    if (UPanelWidget* ExistingParent = ChildWidget->GetParent())
    {
        if (ExistingParent != ParentPanel)
        {
            if (!Request.bReparentIfExists)
            {
                return TResult<FWidgetAddChildResult>::Error(VibeUE::ErrorCodes::OPERATION_NOT_ALLOWED, FString::Printf(TEXT("Child widget '%s' already has a different parent"), *Request.ChildName));
            }

            ExistingParent->RemoveChild(ChildWidget);
            bStructureChanged = true;
            bReparented = true;
        }
    }
    else if (WidgetTree->RootWidget == ChildWidget)
    {
        return TResult<FWidgetAddChildResult>::Error(VibeUE::ErrorCodes::OPERATION_NOT_ALLOWED, TEXT("Cannot reparent the root widget using add_child_to_panel"));
    }

    int32 DesiredIndex = INDEX_NONE;
    if (Request.InsertIndex.IsSet())
    {
        DesiredIndex = FMath::Max(0, Request.InsertIndex.GetValue());
    }

    if (!bAlreadyInParent)
    {
        if (DesiredIndex != INDEX_NONE)
        {
            DesiredIndex = FMath::Min(DesiredIndex, ParentPanel->GetChildrenCount());
            ParentPanel->InsertChildAt(DesiredIndex, ChildWidget);
        }
        else
        {
            ParentPanel->AddChild(ChildWidget);
        }
        bStructureChanged = true;
    }
    else if (DesiredIndex != INDEX_NONE)
    {
        const int32 CurrentIndex = ParentPanel->GetChildIndex(ChildWidget);
        DesiredIndex = FMath::Min(DesiredIndex, ParentPanel->GetChildrenCount() - 1);
        if (CurrentIndex != DesiredIndex && CurrentIndex != INDEX_NONE)
        {
            ParentPanel->RemoveChild(ChildWidget);
            ParentPanel->InsertChildAt(DesiredIndex, ChildWidget);
            bStructureChanged = true;
        }
    }

    bool bSlotPropsApplied = false;
    FString ResolvedSlotType;
    if (Request.SlotProperties.IsValid() && ChildWidget->Slot)
    {
        bSlotPropsApplied = ApplySlotProperties(WidgetBlueprint, ChildWidget, ChildWidget->Slot.Get(), ParentPanel, Request.SlotProperties, ResolvedSlotType);
    }
    else if (ChildWidget->Slot)
    {
        ResolvedSlotType = ChildWidget->Slot->GetClass()->GetName();
    }

    if (bStructureChanged)
    {
        WidgetBlueprint->Modify();
        WidgetBlueprint->MarkPackageDirty();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
        FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
    }

    FWidgetAddChildResult Result;
    Result.WidgetBlueprintName = WidgetBlueprint->GetName();
    Result.ChildName = ChildWidget->GetName();
    Result.ParentName = ParentPanel->GetName();
    Result.ParentType = ParentPanel->GetClass()->GetName();
    Result.bReparented = bReparented;
    Result.bSlotPropertiesApplied = bSlotPropsApplied;
    Result.bStructureChanged = bStructureChanged;
    if (DesiredIndex != INDEX_NONE)
    {
        Result.ChildIndex = ParentPanel->GetChildIndex(ChildWidget);
    }

    return TResult<FWidgetAddChildResult>::Success(Result);
}

TResult<UWidget*> FUMGWidgetService::CreateAndAddWidget(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetClassName, const FString& WidgetName, const FString& ParentName, bool bIsVariable, const TMap<FString, FString>& InitialProperties)
{
    TResult<UWidget*> CreateResult = AddWidgetComponent(WidgetBlueprint, WidgetClassName, WidgetName, ParentName, bIsVariable);
    if (CreateResult.IsError())
    {
        return CreateResult;
    }

    if (InitialProperties.Num() > 0)
    {
        LogWarning(TEXT("InitialProperties not applied - caller should use WidgetPropertyService after creation"));
    }

    return CreateResult;
}

TResult<bool> FUMGWidgetService::ValidateWidgetCreation(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetClassName, const FString& WidgetName, const FString& ParentName)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    UClass* WidgetClass = GetWidgetClass(WidgetClassName);
    if (!WidgetClass)
    {
        return TResult<bool>::Success(false);
    }

    if (!IsWidgetNameUnique(WidgetBlueprint, WidgetName))
    {
        return TResult<bool>::Success(false);
    }

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

TResult<FWidgetRemoveComponentResult> FUMGWidgetService::RemoveComponent(UWidgetBlueprint* WidgetBlueprint, const FWidgetRemoveComponentRequest& Request)
{
    auto BlueprintValidation = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (BlueprintValidation.IsError())
    {
        return TResult<FWidgetRemoveComponentResult>::Error(BlueprintValidation.GetErrorCode(), BlueprintValidation.GetErrorMessage());
    }

    UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
    if (!WidgetTree)
    {
        return TResult<FWidgetRemoveComponentResult>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("WidgetTree not found in widget blueprint"));
    }

    UWidget* TargetComponent = WidgetTree->FindWidget(FName(*Request.ComponentName));
    if (!TargetComponent)
    {
        return TResult<FWidgetRemoveComponentResult>::Error(VibeUE::ErrorCodes::WIDGET_NOT_FOUND, FString::Printf(TEXT("Component '%s' not found"), *Request.ComponentName));
    }

    TArray<UWidget*> CollectedChildren;
    CollectChildWidgets(TargetComponent, CollectedChildren);

    TArray<FWidgetComponentRecord> RemovedComponents;
    TArray<FWidgetComponentRecord> OrphanedChildren;

    if (!Request.bRemoveChildren && CollectedChildren.Num() > 0)
    {
        if (UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
        {
            for (UWidget* Child : CollectedChildren)
            {
                if (!Child)
                {
                    continue;
                }

                if (UWidget* CurrentParent = Child->GetParent())
                {
                    if (UPanelWidget* CurrentPanel = Cast<UPanelWidget>(CurrentParent))
                    {
                        CurrentPanel->RemoveChild(Child);
                    }
                }

                RootPanel->AddChild(Child);

                FWidgetComponentRecord Record;
                Record.Name = Child->GetName();
                Record.Type = Child->GetClass()->GetName();
                OrphanedChildren.Add(Record);
            }
        }
        else
        {
            return TResult<FWidgetRemoveComponentResult>::Error(VibeUE::ErrorCodes::OPERATION_NOT_ALLOWED, TEXT("Root widget is not a panel; cannot reparent children"));
        }
    }

    UWidget* ParentWidget = TargetComponent->GetParent();
    FString ParentName = ParentWidget ? ParentWidget->GetName() : TEXT("Root");
    FString ParentType = ParentWidget ? ParentWidget->GetClass()->GetName() : TEXT("N/A");

    if (ParentWidget)
    {
        if (UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget))
        {
            ParentPanel->RemoveChild(TargetComponent);
        }
        else
        {
            return TResult<FWidgetRemoveComponentResult>::Error(VibeUE::ErrorCodes::OPERATION_NOT_ALLOWED, TEXT("Parent is not a panel widget"));
        }
    }
    else if (WidgetTree->RootWidget == TargetComponent)
    {
        WidgetTree->RootWidget = nullptr;
    }

    FWidgetComponentRecord MainRecord;
    MainRecord.Name = Request.ComponentName;
    MainRecord.Type = TargetComponent->GetClass()->GetName();
    RemovedComponents.Add(MainRecord);

    if (Request.bRemoveChildren)
    {
        for (UWidget* Child : CollectedChildren)
        {
            if (!Child)
            {
                continue;
            }

            FWidgetComponentRecord Record;
            Record.Name = Child->GetName();
            Record.Type = Child->GetClass()->GetName();
            RemovedComponents.Add(Record);
        }
    }

    bool bVariableCleanupPerformed = false;
    if (Request.bRemoveFromVariables)
    {
        for (int32 Index = WidgetBlueprint->NewVariables.Num() - 1; Index >= 0; --Index)
        {
            if (WidgetBlueprint->NewVariables[Index].VarName.ToString() == Request.ComponentName)
            {
                WidgetBlueprint->NewVariables.RemoveAt(Index);
                bVariableCleanupPerformed = true;
                break;
            }
        }
    }

    WidgetBlueprint->Modify();
    WidgetBlueprint->MarkPackageDirty();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

    FWidgetRemoveComponentResult Result;
    Result.WidgetBlueprintName = WidgetBlueprint->GetName();
    Result.ComponentName = Request.ComponentName;
    Result.ParentName = ParentName;
    Result.ParentType = ParentType;
    Result.bVariableCleanupPerformed = bVariableCleanupPerformed;
    Result.bStructureChanged = true;
    Result.RemovedComponents = MoveTemp(RemovedComponents);
    Result.OrphanedChildren = MoveTemp(OrphanedChildren);

    return TResult<FWidgetRemoveComponentResult>::Success(Result);
}

TResult<FWidgetSlotUpdateResult> FUMGWidgetService::SetSlotProperties(UWidgetBlueprint* WidgetBlueprint, const FWidgetSlotUpdateRequest& Request)
{
    auto BlueprintValidation = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (BlueprintValidation.IsError())
    {
        return TResult<FWidgetSlotUpdateResult>::Error(BlueprintValidation.GetErrorCode(), BlueprintValidation.GetErrorMessage());
    }

    UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
    if (!WidgetTree)
    {
        return TResult<FWidgetSlotUpdateResult>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("WidgetTree not found in widget blueprint"));
    }

    UWidget* TargetWidget = WidgetTree->FindWidget(FName(*Request.WidgetName));
    if (!TargetWidget)
    {
        return TResult<FWidgetSlotUpdateResult>::Error(VibeUE::ErrorCodes::WIDGET_NOT_FOUND, FString::Printf(TEXT("Target widget '%s' not found"), *Request.WidgetName));
    }

    UPanelSlot* PanelSlot = TargetWidget->Slot;
    if (!PanelSlot)
    {
        return TResult<FWidgetSlotUpdateResult>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, TEXT("Widget does not have a panel slot"));
    }

    UPanelWidget* ParentPanel = Cast<UPanelWidget>(TargetWidget->GetParent());
    if (!ParentPanel)
    {
        return TResult<FWidgetSlotUpdateResult>::Error(VibeUE::ErrorCodes::COMPONENT_NOT_FOUND, TEXT("Parent widget is not a panel"));
    }

    FString ResolvedSlotType;
    const bool bApplied = ApplySlotProperties(WidgetBlueprint, TargetWidget, PanelSlot, ParentPanel, Request.SlotProperties, ResolvedSlotType);

    WidgetBlueprint->MarkPackageDirty();
    FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

    FWidgetSlotUpdateResult Result;
    Result.WidgetBlueprintName = WidgetBlueprint->GetName();
    Result.WidgetName = TargetWidget->GetName();
    Result.SlotType = Request.SlotTypeOverride.IsEmpty() ? ResolvedSlotType : Request.SlotTypeOverride;
    Result.bApplied = bApplied;
    Result.AppliedProperties = Request.SlotProperties;

    return TResult<FWidgetSlotUpdateResult>::Success(Result);
}

TResult<UPanelWidget*> FUMGWidgetService::GetParentPanel(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
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

    if (UPanelWidget* ParentPanel = Cast<UPanelWidget>(Widget->GetParent()))
    {
        return TResult<UPanelWidget*>::Success(ParentPanel);
    }

    return TResult<UPanelWidget*>::Error(VibeUE::ErrorCodes::COMPONENT_NOT_FOUND, TEXT("Parent widget is not a panel"));
}

TResult<FWidgetComponentInfo> FUMGWidgetService::GetWidgetComponentInfo(UWidgetBlueprint* WidgetBlueprint, const FString& ComponentName, bool bIncludeSlotInfo)
{
    auto Validation = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (Validation.IsError())
    {
        return TResult<FWidgetComponentInfo>::Error(Validation.GetErrorCode(), Validation.GetErrorMessage());
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<FWidgetComponentInfo>::Error(VibeUE::ErrorCodes::WIDGET_NOT_FOUND, TEXT("Widget blueprint has no widget tree"));
    }

    UWidget* Target = WidgetBlueprint->WidgetTree->FindWidget(FName(*ComponentName));
    if (!Target)
    {
        return TResult<FWidgetComponentInfo>::Error(VibeUE::ErrorCodes::WIDGET_NOT_FOUND, FString::Printf(TEXT("Component '%s' not found"), *ComponentName));
    }

    FWidgetComponentInfo Info;
    Info.Name = Target->GetName();
    Info.Type = Target->GetClass() ? Target->GetClass()->GetName() : FString();
    Info.bIsVariable = Target->bIsVariable;
    Info.bIsEnabled = Target->GetIsEnabled();
    Info.Visibility = UEnum::GetValueAsString(Target->GetVisibility());

    if (bIncludeSlotInfo && Target->Slot)
    {
        FWidgetSlotInfo Slot;
        Slot.WidgetName = Target->GetName();
        Slot.SlotType = Target->Slot->GetClass() ? Target->Slot->GetClass()->GetName() : FString();

        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Target->Slot))
        {
            Slot.Properties.Add(TEXT("position_x"), FString::SanitizeFloat(CanvasSlot->GetPosition().X));
            Slot.Properties.Add(TEXT("position_y"), FString::SanitizeFloat(CanvasSlot->GetPosition().Y));
            Slot.Properties.Add(TEXT("size_x"), FString::SanitizeFloat(CanvasSlot->GetSize().X));
            Slot.Properties.Add(TEXT("size_y"), FString::SanitizeFloat(CanvasSlot->GetSize().Y));
            Slot.Properties.Add(TEXT("auto_size"), CanvasSlot->GetAutoSize() ? TEXT("true") : TEXT("false"));
            Slot.Properties.Add(TEXT("z_order"), FString::FromInt(CanvasSlot->GetZOrder()));
            const FAnchors Anchors = CanvasSlot->GetAnchors();
            Slot.Properties.Add(TEXT("anchor_min_x"), FString::SanitizeFloat(Anchors.Minimum.X));
            Slot.Properties.Add(TEXT("anchor_min_y"), FString::SanitizeFloat(Anchors.Minimum.Y));
            Slot.Properties.Add(TEXT("anchor_max_x"), FString::SanitizeFloat(Anchors.Maximum.X));
            Slot.Properties.Add(TEXT("anchor_max_y"), FString::SanitizeFloat(Anchors.Maximum.Y));
        }
        else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Target->Slot))
        {
            Slot.Properties.Add(TEXT("padding"), TEXT("horizontal_box_padding"));
            // more detailed padding parsing could be added
        }
        else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Target->Slot))
        {
            Slot.Properties.Add(TEXT("padding"), TEXT("vertical_box_padding"));
        }
        else if (UOverlaySlot* OSlot = Cast<UOverlaySlot>(Target->Slot))
        {
            Slot.Properties.Add(TEXT("padding"), TEXT("overlay_padding"));
        }
        else if (UScrollBoxSlot* SSlot = Cast<UScrollBoxSlot>(Target->Slot))
        {
            Slot.Properties.Add(TEXT("padding"), TEXT("scrollbox_padding"));
        }

        Info.SlotInfo = Slot;
    }

    return TResult<FWidgetComponentInfo>::Success(Info);
}

UClass* FUMGWidgetService::GetWidgetClass(const FString& WidgetClassName)
{
    UClass* WidgetClass = FindFirstObjectSafe<UClass>(*WidgetClassName);
    if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
    {
        return nullptr;
    }
    return WidgetClass;
}

bool FUMGWidgetService::IsWidgetNameUnique(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
{
    if (!WidgetBlueprint->WidgetTree)
    {
        return true;
    }

    return WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName)) == nullptr;
}

TResult<UPanelWidget*> FUMGWidgetService::ResolveParentPanel(UWidgetBlueprint* WidgetBlueprint, const FString& ParentName, const FString& ParentType)
{
    auto BlueprintValidation = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (BlueprintValidation.IsError())
    {
        return TResult<UPanelWidget*>::Error(BlueprintValidation.GetErrorCode(), BlueprintValidation.GetErrorMessage());
    }

    UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
    if (!WidgetTree)
    {
        return TResult<UPanelWidget*>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("WidgetTree not found in widget blueprint"));
    }

    if (ParentName.IsEmpty())
    {
        if (UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
        {
            return TResult<UPanelWidget*>::Success(RootPanel);
        }
        return TResult<UPanelWidget*>::Error(VibeUE::ErrorCodes::COMPONENT_NOT_FOUND, TEXT("Root widget is not a panel"));
    }

    if (UWidget* ExistingParent = WidgetTree->FindWidget(FName(*ParentName)))
    {
        if (UPanelWidget* ExistingPanel = Cast<UPanelWidget>(ExistingParent))
        {
            return TResult<UPanelWidget*>::Success(ExistingPanel);
        }
        return TResult<UPanelWidget*>::Error(VibeUE::ErrorCodes::COMPONENT_TYPE_INVALID, FString::Printf(TEXT("Parent widget '%s' is not a panel"), *ParentName));
    }

    UPanelWidget* NewParent = nullptr;
    if (ParentType == TEXT("CanvasPanel"))
    {
        NewParent = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *ParentName);
    }
    else if (ParentType == TEXT("Overlay"))
    {
        NewParent = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *ParentName);
    }
    else if (ParentType == TEXT("HorizontalBox"))
    {
        NewParent = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *ParentName);
    }
    else if (ParentType == TEXT("VerticalBox"))
    {
        NewParent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *ParentName);
    }
    else if (ParentType == TEXT("ScrollBox"))
    {
        NewParent = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), *ParentName);
    }
    else if (ParentType == TEXT("GridPanel"))
    {
        NewParent = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), *ParentName);
    }
    else if (ParentType == TEXT("UniformGridPanel"))
    {
        NewParent = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), *ParentName);
    }
    else if (ParentType == TEXT("WidgetSwitcher"))
    {
        NewParent = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), *ParentName);
    }

    if (NewParent)
    {
        if (UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
        {
            RootPanel->AddChild(NewParent);
        }
        return TResult<UPanelWidget*>::Success(NewParent);
    }

    return TResult<UPanelWidget*>::Error(VibeUE::ErrorCodes::COMPONENT_TYPE_INVALID, FString::Printf(TEXT("Unsupported parent panel type '%s'"), *ParentType));
}

bool FUMGWidgetService::ApplySlotProperties(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget, UPanelSlot* PanelSlot, UPanelWidget* ParentPanel, const TSharedPtr<FJsonObject>& SlotProperties, FString& OutSlotType)
{
    if (!SlotProperties.IsValid())
    {
        OutSlotType = PanelSlot ? PanelSlot->GetClass()->GetName() : FString();
        return false;
    }

    if (!Widget || !PanelSlot || !ParentPanel)
    {
        return false;
    }

    OutSlotType = PanelSlot->GetClass()->GetName();

    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
    {
        const TArray<TSharedPtr<FJsonValue>>* PositionArray;
        if (SlotProperties->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray->Num() >= 2)
        {
            CanvasSlot->SetPosition(FVector2D((*PositionArray)[0]->AsNumber(), (*PositionArray)[1]->AsNumber()));
        }

        const TArray<TSharedPtr<FJsonValue>>* SizeArray;
        if (SlotProperties->TryGetArrayField(TEXT("size"), SizeArray) && SizeArray->Num() >= 2)
        {
            CanvasSlot->SetSize(FVector2D((*SizeArray)[0]->AsNumber(), (*SizeArray)[1]->AsNumber()));
        }

        const TSharedPtr<FJsonObject>* AnchorsObj;
        if (SlotProperties->TryGetObjectField(TEXT("anchors"), AnchorsObj))
        {
            FAnchors Anchors;
            Anchors.Minimum.X = (*AnchorsObj)->GetNumberField(TEXT("min_x"));
            Anchors.Minimum.Y = (*AnchorsObj)->GetNumberField(TEXT("min_y"));
            Anchors.Maximum.X = (*AnchorsObj)->GetNumberField(TEXT("max_x"));
            Anchors.Maximum.Y = (*AnchorsObj)->GetNumberField(TEXT("max_y"));
            CanvasSlot->SetAnchors(Anchors);
        }

        const TArray<TSharedPtr<FJsonValue>>* AlignmentArray;
        if (SlotProperties->TryGetArrayField(TEXT("alignment"), AlignmentArray) && AlignmentArray->Num() >= 2)
        {
            CanvasSlot->SetAlignment(FVector2D((*AlignmentArray)[0]->AsNumber(), (*AlignmentArray)[1]->AsNumber()));
        }

        if (SlotProperties->HasField(TEXT("auto_size")))
        {
            CanvasSlot->SetAutoSize(SlotProperties->GetBoolField(TEXT("auto_size")));
        }

        if (SlotProperties->HasField(TEXT("z_order")))
        {
            CanvasSlot->SetZOrder(static_cast<int32>(SlotProperties->GetNumberField(TEXT("z_order"))));
        }

        return true;
    }

    const auto ResolveAlignment = [](const FString& Value) -> EHorizontalAlignment
    {
        if (Value.Equals(TEXT("Left"), ESearchCase::IgnoreCase))
        {
            return HAlign_Left;
        }
        if (Value.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
        {
            return HAlign_Center;
        }
        if (Value.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
        {
            return HAlign_Right;
        }
        return HAlign_Fill;
    };

    const auto ResolveVerticalAlignment = [](const FString& Value) -> EVerticalAlignment
    {
        if (Value.Equals(TEXT("Top"), ESearchCase::IgnoreCase))
        {
            return VAlign_Top;
        }
        if (Value.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
        {
            return VAlign_Center;
        }
        if (Value.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase))
        {
            return VAlign_Bottom;
        }
        return VAlign_Fill;
    };

    const auto ApplyPadding = [](UPanelSlot* Slot, const TSharedPtr<FJsonObject>& Properties)
    {
        const TArray<TSharedPtr<FJsonValue>>* PaddingArray;
        if (Properties->TryGetArrayField(TEXT("padding"), PaddingArray) && PaddingArray->Num() >= 4)
        {
            if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
            {
                HorizontalSlot->SetPadding(FMargin((*PaddingArray)[0]->AsNumber(), (*PaddingArray)[1]->AsNumber(), (*PaddingArray)[2]->AsNumber(), (*PaddingArray)[3]->AsNumber()));
            }
            else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
            {
                VerticalSlot->SetPadding(FMargin((*PaddingArray)[0]->AsNumber(), (*PaddingArray)[1]->AsNumber(), (*PaddingArray)[2]->AsNumber(), (*PaddingArray)[3]->AsNumber()));
            }
            else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
            {
                OverlaySlot->SetPadding(FMargin((*PaddingArray)[0]->AsNumber(), (*PaddingArray)[1]->AsNumber(), (*PaddingArray)[2]->AsNumber(), (*PaddingArray)[3]->AsNumber()));
            }
            else if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(Slot))
            {
                ScrollSlot->SetPadding(FMargin((*PaddingArray)[0]->AsNumber(), (*PaddingArray)[1]->AsNumber(), (*PaddingArray)[2]->AsNumber(), (*PaddingArray)[3]->AsNumber()));
            }
            else if (UWidgetSwitcherSlot* SwitcherSlot = Cast<UWidgetSwitcherSlot>(Slot))
            {
                SwitcherSlot->SetPadding(FMargin((*PaddingArray)[0]->AsNumber(), (*PaddingArray)[1]->AsNumber(), (*PaddingArray)[2]->AsNumber(), (*PaddingArray)[3]->AsNumber()));
            }
        }
    };

    ApplyPadding(PanelSlot, SlotProperties);

    FString HorizontalAlignmentValue;
    if (SlotProperties->TryGetStringField(TEXT("horizontal_alignment"), HorizontalAlignmentValue) || SlotProperties->TryGetStringField(TEXT("HorizontalAlignment"), HorizontalAlignmentValue))
    {
        if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
        {
            HorizontalSlot->SetHorizontalAlignment(ResolveAlignment(HorizontalAlignmentValue));
        }
        else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(PanelSlot))
        {
            VerticalSlot->SetHorizontalAlignment(ResolveAlignment(HorizontalAlignmentValue));
        }
        else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PanelSlot))
        {
            OverlaySlot->SetHorizontalAlignment(ResolveAlignment(HorizontalAlignmentValue));
        }
        else if (UWidgetSwitcherSlot* SwitcherSlot = Cast<UWidgetSwitcherSlot>(PanelSlot))
        {
            SwitcherSlot->SetHorizontalAlignment(ResolveAlignment(HorizontalAlignmentValue));
        }
        else if (USizeBoxSlot* SizeBoxSlot = Cast<USizeBoxSlot>(PanelSlot))
        {
            SizeBoxSlot->SetHorizontalAlignment(ResolveAlignment(HorizontalAlignmentValue));
        }
    }

    FString VerticalAlignmentValue;
    if (SlotProperties->TryGetStringField(TEXT("vertical_alignment"), VerticalAlignmentValue) || SlotProperties->TryGetStringField(TEXT("VerticalAlignment"), VerticalAlignmentValue))
    {
        if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
        {
            HorizontalSlot->SetVerticalAlignment(ResolveVerticalAlignment(VerticalAlignmentValue));
        }
        else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(PanelSlot))
        {
            VerticalSlot->SetVerticalAlignment(ResolveVerticalAlignment(VerticalAlignmentValue));
        }
        else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PanelSlot))
        {
            OverlaySlot->SetVerticalAlignment(ResolveVerticalAlignment(VerticalAlignmentValue));
        }
        else if (UWidgetSwitcherSlot* SwitcherSlot = Cast<UWidgetSwitcherSlot>(PanelSlot))
        {
            SwitcherSlot->SetVerticalAlignment(ResolveVerticalAlignment(VerticalAlignmentValue));
        }
        else if (USizeBoxSlot* SizeBoxSlot = Cast<USizeBoxSlot>(PanelSlot))
        {
            SizeBoxSlot->SetVerticalAlignment(ResolveVerticalAlignment(VerticalAlignmentValue));
        }
    }

    if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(PanelSlot))
    {
        FString SizeRule;
        if (SlotProperties->TryGetStringField(TEXT("SizeRule"), SizeRule) || SlotProperties->TryGetStringField(TEXT("size_rule"), SizeRule))
        {
            if (SizeRule.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
            {
                ScrollSlot->SetSize(ESlateSizeRule::Fill);
            }
            else if (SizeRule.Equals(TEXT("Auto"), ESearchCase::IgnoreCase) || SizeRule.Equals(TEXT("Automatic"), ESearchCase::IgnoreCase))
            {
                ScrollSlot->SetSize(ESlateSizeRule::Automatic);
            }
        }
    }

    if (UGridSlot* GridSlot = Cast<UGridSlot>(PanelSlot))
    {
        if (SlotProperties->HasField(TEXT("row")))
        {
            GridSlot->SetRow(static_cast<int32>(SlotProperties->GetNumberField(TEXT("row"))));
        }
        if (SlotProperties->HasField(TEXT("column")))
        {
            GridSlot->SetColumn(static_cast<int32>(SlotProperties->GetNumberField(TEXT("column"))));
        }
        if (SlotProperties->HasField(TEXT("row_span")))
        {
            GridSlot->SetRowSpan(static_cast<int32>(SlotProperties->GetNumberField(TEXT("row_span"))));
        }
        if (SlotProperties->HasField(TEXT("column_span")))
        {
            GridSlot->SetColumnSpan(static_cast<int32>(SlotProperties->GetNumberField(TEXT("column_span"))));
        }
    }

    if (UUniformGridSlot* UniformSlot = Cast<UUniformGridSlot>(PanelSlot))
    {
        if (SlotProperties->HasField(TEXT("row")))
        {
            UniformSlot->SetRow(static_cast<int32>(SlotProperties->GetNumberField(TEXT("row"))));
        }
        if (SlotProperties->HasField(TEXT("column")))
        {
            UniformSlot->SetColumn(static_cast<int32>(SlotProperties->GetNumberField(TEXT("column"))));
        }
        if (SlotProperties->HasField(TEXT("horizontal_alignment")))
        {
            UniformSlot->SetHorizontalAlignment(ResolveAlignment(SlotProperties->GetStringField(TEXT("horizontal_alignment"))));
        }
        if (SlotProperties->HasField(TEXT("vertical_alignment")))
        {
            UniformSlot->SetVerticalAlignment(ResolveVerticalAlignment(SlotProperties->GetStringField(TEXT("vertical_alignment"))));
        }
    }

    return true;
}

void FUMGWidgetService::CollectChildWidgets(UWidget* Widget, TArray<UWidget*>& OutChildren) const
{
    if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
    {
        for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
        {
            if (UWidget* Child = Panel->GetChildAt(ChildIndex))
            {
                OutChildren.Add(Child);
                CollectChildWidgets(Child, OutChildren);
            }
        }
    }
}
