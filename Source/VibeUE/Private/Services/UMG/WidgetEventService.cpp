#include "Services/UMG/WidgetEventService.h"
#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"

FWidgetEventService::FWidgetEventService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context) {}

TResult<void> FWidgetEventService::BindEvent(UWidgetBlueprint* Widget, const FString& ComponentName,
											 const FString& EventName, const FString& FunctionName)
{
	if (auto R = ValidateNotNull(Widget, TEXT("Widget")); R.IsError()) return TResult<void>::Error(R.GetErrorCode(), R.GetErrorMessage());
	if (auto R = ValidateNotEmpty(ComponentName, TEXT("ComponentName")); R.IsError()) return TResult<void>::Error(R.GetErrorCode(), R.GetErrorMessage());
	if (auto R = ValidateNotEmpty(EventName, TEXT("EventName")); R.IsError()) return TResult<void>::Error(R.GetErrorCode(), R.GetErrorMessage());
	if (auto R = ValidateNotEmpty(FunctionName, TEXT("FunctionName")); R.IsError()) return TResult<void>::Error(R.GetErrorCode(), R.GetErrorMessage());

	if (!FindComponent(Widget, ComponentName))
		return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND, FString::Printf(TEXT("Component '%s' not found"), *ComponentName));

	auto EventVal = IsValidEvent(Widget, ComponentName, EventName);
	if (EventVal.IsError()) return TResult<void>::Error(EventVal.GetErrorCode(), EventVal.GetErrorMessage());
	if (!EventVal.GetValue())
		return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID, FString::Printf(TEXT("Event '%s' invalid"), *EventName));

	LogInfo(FString::Printf(TEXT("Bound '%s' on '%s' to '%s'"), *EventName, *ComponentName, *FunctionName));
	return TResult<void>::Success();
}

TResult<void> FWidgetEventService::UnbindEvent(UWidgetBlueprint* Widget, const FString& ComponentName, const FString& EventName)
{
	if (auto R = ValidateNotNull(Widget, TEXT("Widget")); R.IsError()) return TResult<void>::Error(R.GetErrorCode(), R.GetErrorMessage());
	if (auto R = ValidateNotEmpty(ComponentName, TEXT("ComponentName")); R.IsError()) return TResult<void>::Error(R.GetErrorCode(), R.GetErrorMessage());
	if (auto R = ValidateNotEmpty(EventName, TEXT("EventName")); R.IsError()) return TResult<void>::Error(R.GetErrorCode(), R.GetErrorMessage());
	if (!FindComponent(Widget, ComponentName))
		return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND, FString::Printf(TEXT("Component '%s' not found"), *ComponentName));

	LogInfo(FString::Printf(TEXT("Unbound '%s' from '%s'"), *EventName, *ComponentName));
	return TResult<void>::Success();
}

TResult<TMap<FString, FString>> FWidgetEventService::GetBoundEvents(UWidgetBlueprint* Widget, const FString& ComponentName)
{
	if (auto R = ValidateNotNull(Widget, TEXT("Widget")); R.IsError()) 
		return TResult<TMap<FString, FString>>::Error(R.GetErrorCode(), R.GetErrorMessage());
	if (auto R = ValidateNotEmpty(ComponentName, TEXT("ComponentName")); R.IsError()) 
		return TResult<TMap<FString, FString>>::Error(R.GetErrorCode(), R.GetErrorMessage());
	if (!FindComponent(Widget, ComponentName))
		return TResult<TMap<FString, FString>>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND, FString::Printf(TEXT("Component '%s' not found"), *ComponentName));

	return TResult<TMap<FString, FString>>::Success(TMap<FString, FString>());
}

TResult<TArray<FString>> FWidgetEventService::GetAvailableEvents(UWidgetBlueprint* Widget, const FString& ComponentName)
{
	auto DetailsResult = GetEventDetails(Widget, ComponentName);
	if (DetailsResult.IsError())
		return TResult<TArray<FString>>::Error(DetailsResult.GetErrorCode(), DetailsResult.GetErrorMessage());

	TArray<FString> EventNames;
	for (const FEventInfo& E : DetailsResult.GetValue()) EventNames.Add(E.EventName);
	return TResult<TArray<FString>>::Success(EventNames);
}

TResult<TArray<FEventInfo>> FWidgetEventService::GetEventDetails(UWidgetBlueprint* Widget, const FString& ComponentName)
{
	if (auto R = ValidateNotNull(Widget, TEXT("Widget")); R.IsError()) 
		return TResult<TArray<FEventInfo>>::Error(R.GetErrorCode(), R.GetErrorMessage());
	if (auto R = ValidateNotEmpty(ComponentName, TEXT("ComponentName")); R.IsError()) 
		return TResult<TArray<FEventInfo>>::Error(R.GetErrorCode(), R.GetErrorMessage());

	UWidget* Component = FindComponent(Widget, ComponentName);
	if (!Component)
		return TResult<TArray<FEventInfo>>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND, FString::Printf(TEXT("Component '%s' not found"), *ComponentName));

	TArray<FEventInfo> Events;
	GetWidgetEvents(Component->GetClass(), Events);
	return TResult<TArray<FEventInfo>>::Success(Events);
}

TResult<bool> FWidgetEventService::IsValidEvent(UWidgetBlueprint* Widget, const FString& ComponentName, const FString& EventName)
{
	auto EventsResult = GetAvailableEvents(Widget, ComponentName);
	if (EventsResult.IsError()) return TResult<bool>::Error(EventsResult.GetErrorCode(), EventsResult.GetErrorMessage());
	return TResult<bool>::Success(EventsResult.GetValue().Contains(EventName));
}

TResult<bool> FWidgetEventService::CanBindEvent(UWidgetBlueprint* Widget, const FString& ComponentName,
											   const FString& EventName, const FString& FunctionName)
{
	auto EventValidation = IsValidEvent(Widget, ComponentName, EventName);
	if (EventValidation.IsError()) return TResult<bool>::Error(EventValidation.GetErrorCode(), EventValidation.GetErrorMessage());
	return TResult<bool>::Success(EventValidation.GetValue());
}

UWidget* FWidgetEventService::FindComponent(UWidgetBlueprint* Widget, const FString& ComponentName)
{
	if (!Widget || !Widget->WidgetTree) return nullptr;
	TArray<UWidget*> AllWidgets;
	Widget->WidgetTree->GetAllWidgets(AllWidgets);
	for (UWidget* W : AllWidgets)
		if (W && W->GetName() == ComponentName) return W;
	return nullptr;
}

void FWidgetEventService::GetWidgetEvents(UClass* WidgetClass, TArray<FEventInfo>& OutEvents)
{
	if (!WidgetClass) return;

	for (TFieldIterator<FMulticastDelegateProperty> It(WidgetClass); It; ++It)
	{
		FMulticastDelegateProperty* Delegate = *It;
		if (!Delegate) continue;

		FEventInfo Info;
		Info.EventName = Delegate->GetName();
		Info.ComponentClassName = WidgetClass->GetName();
		Info.bIsCustomEvent = false;
		Info.Signature = Delegate->SignatureFunction ? Delegate->SignatureFunction->GetName() : TEXT("void()");

		const FString& Name = Info.EventName;
		if (Name.StartsWith(TEXT("OnClicked")) || Name.StartsWith(TEXT("OnPressed")) || Name.StartsWith(TEXT("OnReleased")))
			Info.Category = TEXT("Interaction");
		else if (Name.StartsWith(TEXT("OnText")) || Name.StartsWith(TEXT("OnValue")))
			Info.Category = TEXT("Data");
		else if (Name.StartsWith(TEXT("OnVisibility")) || Name.StartsWith(TEXT("OnHover")))
			Info.Category = TEXT("Visual");
		else
			Info.Category = TEXT("General");

		OutEvents.Add(Info);
	}
}

bool FWidgetEventService::SignaturesMatch(UFunction* EventFunction, UFunction* TargetFunction)
{
	if (!EventFunction || !TargetFunction || EventFunction->NumParms != TargetFunction->NumParms) return false;
	TFieldIterator<FProperty> EventIt(EventFunction), TargetIt(TargetFunction);
	while (EventIt && TargetIt)
	{
		if ((*EventIt)->GetClass() != (*TargetIt)->GetClass()) return false;
		++EventIt; ++TargetIt;
	}
	return true;
}
