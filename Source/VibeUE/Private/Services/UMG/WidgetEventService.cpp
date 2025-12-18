// Copyright Kevin Buckley 2025 All Rights Reserved.

/**
 * WidgetEventService.cpp
 * Clean implementation matching Public/Services/UMG/WidgetEventService.h
 */

#include "Services/UMG/WidgetEventService.h"
#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#include "Components/Widget.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"

FWidgetEventService::FWidgetEventService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<TArray<FWidgetEventInfo>> FWidgetEventService::GetAvailableEvents(UWidgetBlueprint* WidgetBlueprint, const FString& ComponentName, const FString& WidgetType)
{
    auto Validation = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (Validation.IsError())
    {
        return TResult<TArray<FWidgetEventInfo>>::Error(Validation.GetErrorCode(), Validation.GetErrorMessage());
    }

    TArray<FWidgetEventInfo> Events;
    UClass* TargetClass = nullptr;
    FString SourceName;

    // If a component name is provided, find that component and get events from its class
    if (!ComponentName.IsEmpty() && WidgetBlueprint->WidgetTree)
    {
        UWidget* Component = WidgetBlueprint->WidgetTree->FindWidget(FName(*ComponentName));
        if (Component)
        {
            TargetClass = Component->GetClass();
            SourceName = ComponentName;
            
            // For specific widget types, add their well-known delegate events first
            // These are the events that can be bound in Blueprints
            if (UButton* Button = Cast<UButton>(Component))
            {
                // Button-specific delegate events
                FWidgetEventInfo ClickedInfo;
                ClickedInfo.Name = TEXT("OnClicked");
                ClickedInfo.Type = TEXT("Button");
                ClickedInfo.Description = TEXT("Called when the button is clicked. Signature: void OnClicked()");
                Events.Add(ClickedInfo);
                
                FWidgetEventInfo PressedInfo;
                PressedInfo.Name = TEXT("OnPressed");
                PressedInfo.Type = TEXT("Button");
                PressedInfo.Description = TEXT("Called when the button is pressed. Signature: void OnPressed()");
                Events.Add(PressedInfo);
                
                FWidgetEventInfo ReleasedInfo;
                ReleasedInfo.Name = TEXT("OnReleased");
                ReleasedInfo.Type = TEXT("Button");
                ReleasedInfo.Description = TEXT("Called when the button is released. Signature: void OnReleased()");
                Events.Add(ReleasedInfo);
                
                FWidgetEventInfo HoveredInfo;
                HoveredInfo.Name = TEXT("OnHovered");
                HoveredInfo.Type = TEXT("Button");
                HoveredInfo.Description = TEXT("Called when mouse enters the button. Signature: void OnHovered()");
                Events.Add(HoveredInfo);
                
                FWidgetEventInfo UnhoveredInfo;
                UnhoveredInfo.Name = TEXT("OnUnhovered");
                UnhoveredInfo.Type = TEXT("Button");
                UnhoveredInfo.Description = TEXT("Called when mouse leaves the button. Signature: void OnUnhovered()");
                Events.Add(UnhoveredInfo);
            }
            
            // Look for multicast delegate properties (events) on the component class
            for (TFieldIterator<FMulticastDelegateProperty> PropIt(TargetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
            {
                FMulticastDelegateProperty* DelegateProp = *PropIt;
                FString PropName = DelegateProp->GetName();
                
                // Skip if we already added this as a well-known event
                bool bAlreadyAdded = false;
                for (const FWidgetEventInfo& Existing : Events)
                {
                    if (Existing.Name == PropName)
                    {
                        bAlreadyAdded = true;
                        break;
                    }
                }
                
                if (!bAlreadyAdded)
                {
                    FWidgetEventInfo Info;
                    Info.Name = PropName;
                    Info.Type = TargetClass->GetName();
                    Info.Description = TEXT("Multicast delegate - bindable event");
                    Events.Add(Info);
                }
            }
            
            return TResult<TArray<FWidgetEventInfo>>::Success(Events);
        }
        else
        {
            return TResult<TArray<FWidgetEventInfo>>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND, 
                FString::Printf(TEXT("Component '%s' not found in widget"), *ComponentName));
        }
    }

    // Fallback: get widget-level events (original behavior)
    if (!WidgetType.IsEmpty())
    {
        TargetClass = FindObject<UClass>(nullptr, *WidgetType);
    }
    if (!TargetClass && WidgetBlueprint)
    {
        TargetClass = WidgetBlueprint->GeneratedClass;
    }
    if (!TargetClass)
    {
        TargetClass = UWidget::StaticClass();
    }
    SourceName = TargetClass->GetName();

    for (TFieldIterator<UFunction> FuncIt(TargetClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
    {
        UFunction* Func = *FuncIt;
        if (Func->HasAnyFunctionFlags(FUNC_BlueprintEvent | FUNC_BlueprintCallable))
        {
            FWidgetEventInfo Info;
            Info.Name = Func->GetName();
            Info.Type = SourceName;
            Info.Description = TEXT("Discovered via reflection");
            Events.Add(Info);
        }
    }

    return TResult<TArray<FWidgetEventInfo>>::Success(Events);
}

TResult<int32> FWidgetEventService::BindInputEvents(UWidgetBlueprint* WidgetBlueprint, const TArray<FWidgetInputMapping>& Mappings)
{
    auto Validation = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (Validation.IsError())
    {
        return TResult<int32>::Error(Validation.GetErrorCode(), Validation.GetErrorMessage());
    }

    // Best-effort placeholder: mark the blueprint modified and return the count of mappings.
    if (Mappings.Num() > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
    }

    return TResult<int32>::Success(Mappings.Num());
}
