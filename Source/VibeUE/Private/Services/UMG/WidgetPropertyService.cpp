/**
 * @file WidgetPropertyService.cpp
 * @brief Implementation of widget property management functionality
 * 
 * This service provides property get/set operations for widgets,
 * extracted from UMGCommands.cpp as part of Phase 4 refactoring.
 */

#include "Services/UMG/WidgetPropertyService.h"
#include "Core/ErrorCodes.h"
#include "Components/Widget.h"
#include "Components/PanelSlot.h"
#include "UObject/UnrealType.h"
#include "JsonObjectConverter.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetProperty, Log, All);

FWidgetPropertyService::FWidgetPropertyService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<FString> FWidgetPropertyService::GetWidgetProperty(UWidget* Widget, const FString& PropertyPath)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(PropertyPath, TEXT("PropertyPath"));
    if (ValidationResult.IsError())
    {
        return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    FProperty* Property = nullptr;
    void* Container = nullptr;
    
    if (!FindPropertyByPath(Widget, PropertyPath, Property, Container))
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            FString::Printf(TEXT("Property '%s' not found on widget"), *PropertyPath)
        );
    }

    FString Value = PropertyValueToString(Property, Container);
    return TResult<FString>::Success(Value);
}

TResult<void> FWidgetPropertyService::SetWidgetProperty(UWidget* Widget, const FString& PropertyPath, const FString& Value)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    ValidationResult = ValidateNotEmpty(PropertyPath, TEXT("PropertyPath"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    FProperty* Property = nullptr;
    void* Container = nullptr;
    
    if (!FindPropertyByPath(Widget, PropertyPath, Property, Container))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            FString::Printf(TEXT("Property '%s' not found on widget"), *PropertyPath)
        );
    }

    if (!SetPropertyValueFromString(Property, Container, Value))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
            FString::Printf(TEXT("Failed to set property '%s' to value '%s'"), *PropertyPath, *Value)
        );
    }

    Widget->Modify();
    return TResult<void>::Success();
}

TResult<TArray<FWidgetPropertyInfo>> FWidgetPropertyService::ListWidgetProperties(UWidget* Widget, bool bIncludeSlotProperties)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FWidgetPropertyInfo>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TArray<FWidgetPropertyInfo> Properties;

    // Get widget properties
    for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Transient))
        {
            continue;
        }

        FWidgetPropertyInfo Info;
        Info.PropertyName = Property->GetName();
        Info.PropertyType = Property->GetCPPType();
        Info.Category = Property->GetMetaData(TEXT("Category"));
        Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
        Info.bIsBlueprintVisible = Property->HasAnyPropertyFlags(CPF_BlueprintVisible);
        
        Properties.Add(Info);
    }

    // Add slot properties if requested
    if (bIncludeSlotProperties)
    {
        UPanelSlot* Slot = Widget->Slot;
        if (Slot)
        {
            for (TFieldIterator<FProperty> It(Slot->GetClass()); It; ++It)
            {
                FProperty* Property = *It;
                if (!Property || Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Transient))
                {
                    continue;
                }

                FWidgetPropertyInfo Info;
                Info.PropertyName = FString::Printf(TEXT("Slot.%s"), *Property->GetName());
                Info.PropertyType = Property->GetCPPType();
                Info.Category = TEXT("Slot");
                Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
                
                Properties.Add(Info);
            }
        }
    }

    return TResult<TArray<FWidgetPropertyInfo>>::Success(Properties);
}

TResult<FWidgetPropertyDescriptor> FWidgetPropertyService::GetPropertyDescriptor(UWidget* Widget, const FString& PropertyPath)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetPropertyDescriptor>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    FProperty* Property = nullptr;
    void* Container = nullptr;
    
    if (!FindPropertyByPath(Widget, PropertyPath, Property, Container))
    {
        return TResult<FWidgetPropertyDescriptor>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            FString::Printf(TEXT("Property '%s' not found"), *PropertyPath)
        );
    }

    FWidgetPropertyDescriptor Descriptor;
    Descriptor.Info.PropertyName = PropertyPath;
    Descriptor.Info.PropertyType = Property->GetCPPType();
    Descriptor.Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
    Descriptor.Info.CurrentValue = PropertyValueToString(Property, Container);

    return TResult<FWidgetPropertyDescriptor>::Success(Descriptor);
}

TResult<bool> FWidgetPropertyService::ValidatePropertyValue(UWidget* Widget, const FString& PropertyPath, const FString& Value)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    FProperty* Property = nullptr;
    void* Container = nullptr;
    
    if (!FindPropertyByPath(Widget, PropertyPath, Property, Container))
    {
        return TResult<bool>::Success(false);
    }

    // Basic type validation - check if property accepts the type
    if (!Property)
    {
        return TResult<bool>::Success(false);
    }

    // For now, basic validation only
    // Full implementation would validate:
    // - Numeric ranges
    // - Enum values
    // - Object references
    // - String formats
    return TResult<bool>::Success(true);
}

TResult<TArray<FString>> FWidgetPropertyService::SetPropertiesBatch(UWidget* Widget, const TArray<FWidgetPropertyUpdate>& Updates)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FString>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TArray<FString> Errors;

    for (const FWidgetPropertyUpdate& Update : Updates)
    {
        TResult<void> Result = SetWidgetProperty(Widget, Update.PropertyPath, Update.NewValue);
        if (Result.IsError())
        {
            Errors.Add(FString::Printf(TEXT("Property '%s': %s"), *Update.PropertyPath, *Result.GetErrorMessage()));
        }
    }

    return TResult<TArray<FString>>::Success(Errors);
}

TResult<TArray<FWidgetPropertyInfo>> FWidgetPropertyService::GetSlotProperties(UWidget* Widget)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FWidgetPropertyInfo>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TArray<FWidgetPropertyInfo> Properties;

    UPanelSlot* Slot = Widget->Slot;
    if (!Slot)
    {
        return TResult<TArray<FWidgetPropertyInfo>>::Success(Properties);
    }

    for (TFieldIterator<FProperty> It(Slot->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Transient))
        {
            continue;
        }

        FWidgetPropertyInfo Info;
        Info.PropertyName = Property->GetName();
        Info.PropertyType = Property->GetCPPType();
        Info.Category = TEXT("Slot");
        Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
        
        Properties.Add(Info);
    }

    return TResult<TArray<FWidgetPropertyInfo>>::Success(Properties);
}

TResult<void> FWidgetPropertyService::SetSlotProperty(UWidget* Widget, const FString& PropertyPath, const FString& Value)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    UPanelSlot* Slot = Widget->Slot;
    if (!Slot)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            TEXT("Widget is not in a slot")
        );
    }

    FProperty* Property = FindFProperty<FProperty>(Slot->GetClass(), *PropertyPath);
    if (!Property)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            FString::Printf(TEXT("Slot property '%s' not found"), *PropertyPath)
        );
    }

    void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Slot);
    if (!SetPropertyValueFromString(Property, PropertyValue, Value))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
            FString::Printf(TEXT("Failed to set slot property '%s'"), *PropertyPath)
        );
    }

    Slot->Modify();
    return TResult<void>::Success();
}

bool FWidgetPropertyService::FindPropertyByPath(UObject* Object, const FString& PropertyPath, FProperty*& OutProperty, void*& OutContainer)
{
    if (!Object || PropertyPath.IsEmpty())
    {
        return false;
    }

    // Handle slot properties
    if (PropertyPath.StartsWith(TEXT("Slot.")))
    {
        UWidget* Widget = Cast<UWidget>(Object);
        if (!Widget || !Widget->Slot)
        {
            return false;
        }

        FString SlotPropertyName = PropertyPath.Mid(5); // Remove "Slot."
        OutProperty = FindFProperty<FProperty>(Widget->Slot->GetClass(), *SlotPropertyName);
        if (OutProperty)
        {
            OutContainer = Widget->Slot;
            return true;
        }
        return false;
    }

    // Simple property lookup
    OutProperty = FindFProperty<FProperty>(Object->GetClass(), *PropertyPath);
    if (OutProperty)
    {
        OutContainer = Object;
        return true;
    }

    return false;
}

FString FWidgetPropertyService::PropertyValueToString(FProperty* Property, const void* ValuePtr)
{
    if (!Property || !ValuePtr)
    {
        return TEXT("");
    }

    FString ValueString;
    Property->ExportTextItem_Direct(ValueString, ValuePtr, nullptr, nullptr, PPF_None);
    return ValueString;
}

bool FWidgetPropertyService::SetPropertyValueFromString(FProperty* Property, void* ValuePtr, const FString& ValueString)
{
    if (!Property || !ValuePtr)
    {
        return false;
    }

    const TCHAR* Result = Property->ImportText_Direct(*ValueString, ValuePtr, nullptr, PPF_None);
    return Result != nullptr;
}
