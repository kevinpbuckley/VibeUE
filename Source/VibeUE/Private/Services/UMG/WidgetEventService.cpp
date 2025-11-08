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

FWidgetEventService::FWidgetEventService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<TArray<FWidgetEventInfo>> FWidgetEventService::GetAvailableEvents(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetType)
{
    auto Validation = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (Validation.IsError())
    {
        return TResult<TArray<FWidgetEventInfo>>::Error(Validation.GetErrorCode(), Validation.GetErrorMessage());
    }

    TArray<FWidgetEventInfo> Events;

    UClass* WidgetClass = nullptr;
    if (!WidgetType.IsEmpty())
    {
        WidgetClass = FindObject<UClass>(nullptr, *WidgetType);
    }
    if (!WidgetClass && WidgetBlueprint)
    {
        WidgetClass = WidgetBlueprint->GeneratedClass;
    }
    if (!WidgetClass)
    {
        WidgetClass = UWidget::StaticClass();
    }

    for (TFieldIterator<UFunction> FuncIt(WidgetClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
    {
        UFunction* Func = *FuncIt;
        if (Func->HasAnyFunctionFlags(FUNC_BlueprintEvent | FUNC_BlueprintCallable))
        {
            FWidgetEventInfo Info;
            Info.Name = Func->GetName();
            Info.Type = WidgetClass->GetName();
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
