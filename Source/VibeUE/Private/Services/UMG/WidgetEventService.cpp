/**
 * @file WidgetEventService.cpp
 * @brief Implementation of widget event binding and management
 * 
 * This service provides event binding functionality,
 * extracted from UMGCommands.cpp as part of Phase 4 refactoring.
 */

#include "Services/UMG/WidgetEventService.h"
#include "Core/ErrorCodes.h"
#include "Components/Widget.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetEvent, Log, All);

FWidgetEventService::FWidgetEventService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<TArray<FString>> FWidgetEventService::GetAvailableEvents(UWidget* Widget)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FString>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TArray<FString> Events;

    // Iterate through properties looking for multicast delegates
    for (TFieldIterator<FMulticastDelegateProperty> It(Widget->GetClass()); It; ++It)
    {
        FMulticastDelegateProperty* DelegateProp = *It;
        if (DelegateProp && !DelegateProp->HasAnyPropertyFlags(CPF_Parm))
        {
            Events.Add(DelegateProp->GetName());
        }
    }

    return TResult<TArray<FString>>::Success(Events);
}

TResult<void> FWidgetEventService::BindEvent(
    UWidgetBlueprint* WidgetBlueprint,
    const FString& WidgetName,
    const FString& EventName,
    const FString& FunctionName)
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

    ValidationResult = ValidateNotEmpty(EventName, TEXT("EventName"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    ValidationResult = ValidateNotEmpty(FunctionName, TEXT("FunctionName"));
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

    FMulticastDelegateProperty* EventProp = GetEventProperty(Widget, EventName);
    if (!EventProp)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::EVENT_NOT_FOUND,
            FString::Printf(TEXT("Event '%s' not found on widget '%s'"), *EventName, *WidgetName)
        );
    }

    // Note: Full implementation would create event graph nodes and bind them
    // This is a simplified version that validates the event exists
    
    return TResult<void>::Success();
}

TResult<TMap<FString, FString>> FWidgetEventService::GetBoundEvents(
    UWidgetBlueprint* WidgetBlueprint,
    const FString& WidgetName)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<TMap<FString, FString>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(WidgetName, TEXT("WidgetName"));
    if (ValidationResult.IsError())
    {
        return TResult<TMap<FString, FString>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TMap<FString, FString> BoundEvents;

    // Note: Full implementation would inspect event graph for bound events
    // This is a simplified version

    return TResult<TMap<FString, FString>>::Success(BoundEvents);
}

TResult<void> FWidgetEventService::UnbindEvent(
    UWidgetBlueprint* WidgetBlueprint,
    const FString& WidgetName,
    const FString& EventName)
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

    ValidationResult = ValidateNotEmpty(EventName, TEXT("EventName"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    // Note: Full implementation would remove event graph nodes
    // This is a simplified version

    return TResult<void>::Success();
}

TResult<bool> FWidgetEventService::IsEventBound(
    UWidgetBlueprint* WidgetBlueprint,
    const FString& WidgetName,
    const FString& EventName)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(WidgetName, TEXT("WidgetName"));
    if (ValidationResult.IsError())
    {
        return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(EventName, TEXT("EventName"));
    if (ValidationResult.IsError())
    {
        return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    // Note: Full implementation would check event graph
    // This simplified version returns false

    return TResult<bool>::Success(false);
}

FMulticastDelegateProperty* FWidgetEventService::GetEventProperty(UWidget* Widget, const FString& EventName)
{
    if (!Widget)
    {
        return nullptr;
    }

    for (TFieldIterator<FMulticastDelegateProperty> It(Widget->GetClass()); It; ++It)
    {
        FMulticastDelegateProperty* DelegateProp = *It;
        if (DelegateProp && DelegateProp->GetName().Equals(EventName, ESearchCase::IgnoreCase))
        {
            return DelegateProp;
        }
    }

    return nullptr;
}
