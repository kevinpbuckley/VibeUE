#include "Services/Blueprint/BlueprintPropertyService.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/UnrealType.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Commands/CommonUtils.h"
#include "Dom/JsonValue.h"

FBlueprintPropertyService::FBlueprintPropertyService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<FString> FBlueprintPropertyService::GetProperty(UBlueprint* Blueprint, const FString& PropertyName)
{
    if (!Blueprint)
    {
        return TResult<FString>::Error(TEXT("Blueprint is null"));
    }

    if (!Blueprint->GeneratedClass)
    {
        return TResult<FString>::Error(TEXT("Blueprint has no generated class"));
    }

    // Get the default object
    UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
    if (!DefaultObject)
    {
        return TResult<FString>::Error(TEXT("Failed to get default object"));
    }

    // Find the property
    FProperty* Property = FindFProperty<FProperty>(DefaultObject->GetClass(), *PropertyName);
    if (!Property)
    {
        return TResult<FString>::Error(FString::Printf(TEXT("Property '%s' not found in Blueprint"), *PropertyName));
    }

    // Get property value
    void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(DefaultObject);
    if (!PropertyValuePtr)
    {
        return TResult<FString>::Error(TEXT("Failed to access property value"));
    }

    // Export the property value to a string
    FString ValueString;
    Property->ExportTextItem_Direct(ValueString, PropertyValuePtr, PropertyValuePtr, nullptr, PPF_None);
    
    return TResult<FString>::Success(ValueString);
}

TResult<void> FBlueprintPropertyService::SetProperty(UBlueprint* Blueprint, const FString& PropertyName, const FString& PropertyValue)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(TEXT("Blueprint is null"));
    }

    if (!Blueprint->GeneratedClass)
    {
        return TResult<void>::Error(TEXT("Blueprint has no generated class"));
    }

    // Get the default object
    UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
    if (!DefaultObject)
    {
        return TResult<void>::Error(TEXT("Failed to get default object"));
    }

    // Create a JSON value from the string for SetObjectProperty
    TSharedPtr<FJsonValue> JsonValue = MakeShared<FJsonValueString>(PropertyValue);
    
    FString ErrorMessage;
    if (!FCommonUtils::SetObjectProperty(DefaultObject, PropertyName, JsonValue, ErrorMessage))
    {
        return TResult<void>::Error(ErrorMessage);
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    return TResult<void>::Success();
}

TResult<TArray<FPropertyInfo>> FBlueprintPropertyService::ListProperties(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return TResult<TArray<FPropertyInfo>>::Error(TEXT("Blueprint is null"));
    }

    if (!Blueprint->GeneratedClass)
    {
        return TResult<TArray<FPropertyInfo>>::Error(TEXT("Blueprint has no generated class"));
    }

    // Get the default object
    UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
    if (!DefaultObject)
    {
        return TResult<TArray<FPropertyInfo>>::Error(TEXT("Failed to get default object"));
    }

    TArray<FPropertyInfo> Properties;

    // Iterate through all properties in the class
    for (TFieldIterator<FProperty> PropIt(DefaultObject->GetClass()); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        if (!Property)
        {
            continue;
        }

        FPropertyInfo Info;
        PopulatePropertyInfo(Property, DefaultObject, Info);
        Properties.Add(Info);
    }

    return TResult<TArray<FPropertyInfo>>::Success(Properties);
}

TResult<FPropertyInfo> FBlueprintPropertyService::GetPropertyMetadata(UBlueprint* Blueprint, const FString& PropertyName)
{
    if (!Blueprint)
    {
        return TResult<FPropertyInfo>::Error(TEXT("Blueprint is null"));
    }

    if (!Blueprint->GeneratedClass)
    {
        return TResult<FPropertyInfo>::Error(TEXT("Blueprint has no generated class"));
    }

    // Get the default object
    UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
    if (!DefaultObject)
    {
        return TResult<FPropertyInfo>::Error(TEXT("Failed to get default object"));
    }

    // Find the property
    FProperty* Property = FindFProperty<FProperty>(DefaultObject->GetClass(), *PropertyName);
    if (!Property)
    {
        return TResult<FPropertyInfo>::Error(FString::Printf(TEXT("Property '%s' not found in Blueprint"), *PropertyName));
    }

    FPropertyInfo Info;
    PopulatePropertyInfo(Property, DefaultObject, Info);

    return TResult<FPropertyInfo>::Success(Info);
}

void FBlueprintPropertyService::PopulatePropertyInfo(FProperty* Property, UObject* DefaultObject, FPropertyInfo& OutInfo)
{
    OutInfo.PropertyName = Property->GetName();
    OutInfo.PropertyType = Property->GetCPPType();
    OutInfo.PropertyClass = Property->GetClass()->GetName();
    
    // Property metadata
    OutInfo.Category = Property->GetMetaData(TEXT("Category"));
    OutInfo.Tooltip = Property->GetMetaData(TEXT("ToolTip"));
    
    // Property flags
    OutInfo.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
    OutInfo.bIsBlueprintVisible = Property->HasAnyPropertyFlags(CPF_BlueprintVisible);
    OutInfo.bIsBlueprintReadOnly = Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly);
    
    // Get current value from CDO
    void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(DefaultObject);
    if (PropertyValuePtr)
    {
        // Export the property value to a string
        FString ValueString;
        Property->ExportTextItem_Direct(ValueString, PropertyValuePtr, PropertyValuePtr, nullptr, PPF_None);
        OutInfo.CurrentValue = ValueString;
        
        // Try to get default value from archetype
        if (DefaultObject->GetArchetype())
        {
            void* ArchetypeValuePtr = Property->ContainerPtrToValuePtr<void>(DefaultObject->GetArchetype());
            if (ArchetypeValuePtr)
            {
                FString ArchetypeValueString;
                Property->ExportTextItem_Direct(ArchetypeValueString, ArchetypeValuePtr, ArchetypeValuePtr, nullptr, PPF_None);
                OutInfo.DefaultValue = ArchetypeValueString;
            }
        }
    }
    
    // Type-specific metadata
    if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
    {
        if (Property->HasMetaData(TEXT("ClampMin")))
        {
            OutInfo.MinValue = Property->GetMetaData(TEXT("ClampMin"));
        }
        if (Property->HasMetaData(TEXT("ClampMax")))
        {
            OutInfo.MaxValue = Property->GetMetaData(TEXT("ClampMax"));
        }
        if (Property->HasMetaData(TEXT("UIMin")))
        {
            OutInfo.UIMin = Property->GetMetaData(TEXT("UIMin"));
        }
        if (Property->HasMetaData(TEXT("UIMax")))
        {
            OutInfo.UIMax = Property->GetMetaData(TEXT("UIMax"));
        }
    }
    else if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
    {
        if (PropertyValuePtr)
        {
            UObject* ObjectValue = ObjectProp->GetObjectPropertyValue(PropertyValuePtr);
            OutInfo.ObjectClass = ObjectProp->PropertyClass ? ObjectProp->PropertyClass->GetName() : TEXT("None");
            OutInfo.ObjectValue = ObjectValue ? ObjectValue->GetPathName() : TEXT("None");
        }
    }
}
