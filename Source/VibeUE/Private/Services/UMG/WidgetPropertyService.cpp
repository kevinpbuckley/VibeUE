// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/UMG/WidgetPropertyService.h"
#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "UObject/UnrealType.h"
#include "JsonObjectConverter.h"
#include "Kismet2/BlueprintEditorUtils.h"

FWidgetPropertyService::FWidgetPropertyService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

UWidget* FWidgetPropertyService::FindWidgetComponent(UWidgetBlueprint* Widget, const FString& ComponentName)
{
	if (!Widget || !Widget->WidgetTree)
	{
		return nullptr;
	}

	TArray<UWidget*> AllWidgets;
	Widget->WidgetTree->GetAllWidgets(AllWidgets);

	for (UWidget* WidgetComponent : AllWidgets)
	{
		if (WidgetComponent && WidgetComponent->GetName() == ComponentName)
		{
			return WidgetComponent;
		}
	}

	return nullptr;
}

TResult<FString> FWidgetPropertyService::GetProperty(UWidgetBlueprint* Widget, const FString& ComponentName,
                                                     const FString& PropertyName)
{
	// Validate inputs
	TResult<void> ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError())
	{
		return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError())
	{
		return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	ValidationResult = ValidateNotEmpty(PropertyName, TEXT("PropertyName"));
	if (ValidationResult.IsError())
	{
		return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	// Find the widget component
	UWidget* FoundWidget = FindWidgetComponent(Widget, ComponentName);
	if (!FoundWidget)
	{
		return TResult<FString>::Error(
			EErrorCodes::NotFound,
			FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));
	}

	// Find the property
	FProperty* Property = FoundWidget->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		// Try common alias: IsVariable -> bIsVariable
		if (PropertyName.Equals(TEXT("IsVariable"), ESearchCase::IgnoreCase))
		{
			Property = FoundWidget->GetClass()->FindPropertyByName(TEXT("bIsVariable"));
		}
	}

	if (!Property)
	{
		return TResult<FString>::Error(
			EErrorCodes::NotFound,
			FString::Printf(TEXT("Property '%s' not found on component '%s'"), *PropertyName, *ComponentName));
	}

	// Extract the property value
	FString PropertyValue;
	if (!ExtractPropertyValue(Property, FoundWidget, PropertyValue))
	{
		return TResult<FString>::Error(
			EErrorCodes::InvalidOperation,
			FString::Printf(TEXT("Failed to extract value for property '%s'"), *PropertyName));
	}

	return TResult<FString>::Success(PropertyValue);
}

TResult<void> FWidgetPropertyService::SetProperty(UWidgetBlueprint* Widget, const FString& ComponentName,
                                                  const FString& PropertyName, const FString& Value)
{
	// Validate inputs
	TResult<void> ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError())
	{
		return ValidationResult;
	}

	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError())
	{
		return ValidationResult;
	}

	ValidationResult = ValidateNotEmpty(PropertyName, TEXT("PropertyName"));
	if (ValidationResult.IsError())
	{
		return ValidationResult;
	}

	// Find the widget component
	UWidget* FoundWidget = FindWidgetComponent(Widget, ComponentName);
	if (!FoundWidget)
	{
		return TResult<void>::Error(
			EErrorCodes::NotFound,
			FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));
	}

	// Find the property
	FProperty* Property = FoundWidget->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		// Try common alias: IsVariable -> bIsVariable
		if (PropertyName.Equals(TEXT("IsVariable"), ESearchCase::IgnoreCase))
		{
			Property = FoundWidget->GetClass()->FindPropertyByName(TEXT("bIsVariable"));
		}
	}

	if (!Property)
	{
		return TResult<void>::Error(
			EErrorCodes::NotFound,
			FString::Printf(TEXT("Property '%s' not found on component '%s'"), *PropertyName, *ComponentName));
	}

	// Set the property value
	if (!SetPropertyValue(Property, FoundWidget, Value))
	{
		return TResult<void>::Error(
			EErrorCodes::InvalidOperation,
			FString::Printf(TEXT("Failed to set value for property '%s'"), *PropertyName));
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Widget);

	return TResult<void>::Success();
}

TResult<TMap<FString, FString>> FWidgetPropertyService::GetAllProperties(UWidgetBlueprint* Widget,
                                                                        const FString& ComponentName)
{
	// Validate inputs
	TResult<void> ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError())
	{
		return TResult<TMap<FString, FString>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError())
	{
		return TResult<TMap<FString, FString>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	// Find the widget component
	UWidget* FoundWidget = FindWidgetComponent(Widget, ComponentName);
	if (!FoundWidget)
	{
		return TResult<TMap<FString, FString>>::Error(
			EErrorCodes::NotFound,
			FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));
	}

	TMap<FString, FString> Properties;

	// Iterate through all properties
	for (TFieldIterator<FProperty> PropertyIterator(FoundWidget->GetClass()); PropertyIterator; ++PropertyIterator)
	{
		FProperty* Property = *PropertyIterator;
		if (!Property)
		{
			continue;
		}

		// Skip private/protected properties
		if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected))
		{
			continue;
		}

		FString PropertyName = Property->GetName();
		FString PropertyValue;
		
		if (ExtractPropertyValue(Property, FoundWidget, PropertyValue))
		{
			Properties.Add(PropertyName, PropertyValue);
		}
	}

	return TResult<TMap<FString, FString>>::Success(MoveTemp(Properties));
}

TResult<TArray<FPropertyInfo>> FWidgetPropertyService::ListProperties(UWidgetBlueprint* Widget,
                                                                     const FString& ComponentName)
{
	// Validate inputs
	TResult<void> ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError())
	{
		return TResult<TArray<FPropertyInfo>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError())
	{
		return TResult<TArray<FPropertyInfo>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	// Find the widget component
	UWidget* FoundWidget = FindWidgetComponent(Widget, ComponentName);
	if (!FoundWidget)
	{
		return TResult<TArray<FPropertyInfo>>::Error(
			EErrorCodes::NotFound,
			FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));
	}

	TArray<FPropertyInfo> Properties;

	// Iterate through all properties
	for (TFieldIterator<FProperty> PropertyIterator(FoundWidget->GetClass()); PropertyIterator; ++PropertyIterator)
	{
		FProperty* Property = *PropertyIterator;
		if (!Property)
		{
			continue;
		}

		// Skip private/protected properties
		if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected))
		{
			continue;
		}

		FPropertyInfo Info;
		PopulatePropertyInfo(Property, FoundWidget, Info);
		Properties.Add(Info);
	}

	return TResult<TArray<FPropertyInfo>>::Success(MoveTemp(Properties));
}

TResult<FPropertyInfo> FWidgetPropertyService::GetPropertyMetadata(UWidgetBlueprint* Widget,
                                                                   const FString& ComponentName,
                                                                   const FString& PropertyName)
{
	// Validate inputs
	TResult<void> ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError())
	{
		return TResult<FPropertyInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError())
	{
		return TResult<FPropertyInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	ValidationResult = ValidateNotEmpty(PropertyName, TEXT("PropertyName"));
	if (ValidationResult.IsError())
	{
		return TResult<FPropertyInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	// Find the widget component
	UWidget* FoundWidget = FindWidgetComponent(Widget, ComponentName);
	if (!FoundWidget)
	{
		return TResult<FPropertyInfo>::Error(
			EErrorCodes::NotFound,
			FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));
	}

	// Find the property
	FProperty* Property = FoundWidget->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		// Try common alias: IsVariable -> bIsVariable
		if (PropertyName.Equals(TEXT("IsVariable"), ESearchCase::IgnoreCase))
		{
			Property = FoundWidget->GetClass()->FindPropertyByName(TEXT("bIsVariable"));
		}
	}

	if (!Property)
	{
		return TResult<FPropertyInfo>::Error(
			EErrorCodes::NotFound,
			FString::Printf(TEXT("Property '%s' not found on component '%s'"), *PropertyName, *ComponentName));
	}

	FPropertyInfo Info;
	PopulatePropertyInfo(Property, FoundWidget, Info);

	return TResult<FPropertyInfo>::Success(MoveTemp(Info));
}

TResult<bool> FWidgetPropertyService::IsValidProperty(UWidgetBlueprint* Widget, const FString& ComponentName,
                                                      const FString& PropertyName)
{
	// Validate inputs
	TResult<void> ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError())
	{
		return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError())
	{
		return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	ValidationResult = ValidateNotEmpty(PropertyName, TEXT("PropertyName"));
	if (ValidationResult.IsError())
	{
		return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	// Find the widget component
	UWidget* FoundWidget = FindWidgetComponent(Widget, ComponentName);
	if (!FoundWidget)
	{
		return TResult<bool>::Success(false);
	}

	// Find the property
	FProperty* Property = FoundWidget->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		// Try common alias: IsVariable -> bIsVariable
		if (PropertyName.Equals(TEXT("IsVariable"), ESearchCase::IgnoreCase))
		{
			Property = FoundWidget->GetClass()->FindPropertyByName(TEXT("bIsVariable"));
		}
	}

	return TResult<bool>::Success(Property != nullptr);
}

TResult<bool> FWidgetPropertyService::ValidatePropertyValue(const FString& PropertyType, const FString& Value)
{
	// Basic validation based on property type
	if (PropertyType.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
	{
		bool IsValid = Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || 
		               Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) ||
		               Value.Equals(TEXT("0")) || 
		               Value.Equals(TEXT("1"));
		return TResult<bool>::Success(IsValid);
	}
	else if (PropertyType.Equals(TEXT("int"), ESearchCase::IgnoreCase) || 
	         PropertyType.Equals(TEXT("int32"), ESearchCase::IgnoreCase))
	{
		bool IsValid = Value.IsNumeric();
		return TResult<bool>::Success(IsValid);
	}
	else if (PropertyType.Equals(TEXT("float"), ESearchCase::IgnoreCase))
	{
		bool IsValid = Value.IsNumeric() || Value.Contains(TEXT("."));
		return TResult<bool>::Success(IsValid);
	}
	else if (PropertyType.Equals(TEXT("String"), ESearchCase::IgnoreCase) || 
	         PropertyType.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
	{
		// Strings are always valid
		return TResult<bool>::Success(true);
	}

	// For unknown types, assume valid
	return TResult<bool>::Success(true);
}

void FWidgetPropertyService::PopulatePropertyInfo(FProperty* Property, UWidget* WidgetObject, FPropertyInfo& OutInfo)
{
	if (!Property || !WidgetObject)
	{
		return;
	}

	OutInfo.PropertyName = Property->GetName();
	OutInfo.PropertyClass = Property->GetClass()->GetName();

	// Determine property type and extract value
	if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
	{
		OutInfo.PropertyType = TEXT("String");
		OutInfo.CurrentValue = StrProperty->GetPropertyValue_InContainer(WidgetObject);
	}
	else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		OutInfo.PropertyType = TEXT("Text");
		FText TextValue = TextProperty->GetPropertyValue_InContainer(WidgetObject);
		OutInfo.CurrentValue = TextValue.ToString();
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		OutInfo.PropertyType = TEXT("bool");
		bool BoolValue = BoolProperty->GetPropertyValue_InContainer(WidgetObject);
		OutInfo.CurrentValue = BoolValue ? TEXT("true") : TEXT("false");
	}
	else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
	{
		OutInfo.PropertyType = TEXT("float");
		float FloatValue = FloatProperty->GetPropertyValue_InContainer(WidgetObject);
		OutInfo.CurrentValue = FString::SanitizeFloat(FloatValue);
		
		// Extract min/max metadata
		if (Property->HasMetaData(TEXT("ClampMin")))
		{
			OutInfo.MinValue = Property->GetMetaData(TEXT("ClampMin"));
		}
		if (Property->HasMetaData(TEXT("ClampMax")))
		{
			OutInfo.MaxValue = Property->GetMetaData(TEXT("ClampMax"));
		}
	}
	else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
	{
		OutInfo.PropertyType = TEXT("int");
		int32 IntValue = IntProperty->GetPropertyValue_InContainer(WidgetObject);
		OutInfo.CurrentValue = FString::FromInt(IntValue);
		
		// Extract min/max metadata
		if (Property->HasMetaData(TEXT("ClampMin")))
		{
			OutInfo.MinValue = Property->GetMetaData(TEXT("ClampMin"));
		}
		if (Property->HasMetaData(TEXT("ClampMax")))
		{
			OutInfo.MaxValue = Property->GetMetaData(TEXT("ClampMax"));
		}
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		uint8 ByteValue = ByteProperty->GetPropertyValue_InContainer(WidgetObject);
		if (ByteProperty->Enum)
		{
			OutInfo.PropertyType = FString::Printf(TEXT("Enum<%s>"), *ByteProperty->Enum->GetName());
			OutInfo.CurrentValue = ByteProperty->Enum->GetNameStringByValue(ByteValue);
			
			// Populate enum values
			for (int32 i = 0; i < ByteProperty->Enum->NumEnums() - 1; ++i)
			{
				OutInfo.EnumValues.Add(ByteProperty->Enum->GetNameStringByIndex(i));
			}
		}
		else
		{
			OutInfo.PropertyType = TEXT("byte");
			OutInfo.CurrentValue = FString::FromInt(ByteValue);
		}
	}
	else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		if (EnumProperty->GetEnum())
		{
			OutInfo.PropertyType = FString::Printf(TEXT("Enum<%s>"), *EnumProperty->GetEnum()->GetName());
			
			if (EnumProperty->GetUnderlyingProperty())
			{
				uint8* EnumValuePtr = (uint8*)EnumProperty->ContainerPtrToValuePtr<void>(WidgetObject);
				int64 EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumValuePtr);
				OutInfo.CurrentValue = EnumProperty->GetEnum()->GetNameStringByValue(EnumValue);
			}
			
			// Populate enum values
			for (int32 i = 0; i < EnumProperty->GetEnum()->NumEnums() - 1; ++i)
			{
				OutInfo.EnumValues.Add(EnumProperty->GetEnum()->GetNameStringByIndex(i));
			}
		}
		else
		{
			OutInfo.PropertyType = TEXT("EnumProperty");
			OutInfo.CurrentValue = TEXT("UnknownEnum");
		}
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		OutInfo.PropertyType = FString::Printf(TEXT("Struct<%s>"), *StructProperty->Struct->GetName());
		OutInfo.CurrentValue = TEXT("ComplexType");
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FProperty* InnerProp = ArrayProperty->Inner;
		FString InnerType = InnerProp ? InnerProp->GetClass()->GetName() : TEXT("Unknown");
		OutInfo.PropertyType = FString::Printf(TEXT("Array<%s>"), *InnerType);
		OutInfo.CurrentValue = TEXT("Array");
	}
	else
	{
		OutInfo.PropertyType = Property->GetClass()->GetName();
		OutInfo.CurrentValue = TEXT("ComplexType");
	}

	// Set editability flags
	OutInfo.bIsEditable = !Property->HasAnyPropertyFlags(CPF_EditConst);
	OutInfo.bIsBlueprintVisible = Property->HasAnyPropertyFlags(CPF_BlueprintVisible);
	OutInfo.bIsBlueprintReadOnly = Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly);

	// Extract metadata
	if (Property->HasMetaData(TEXT("Category")))
	{
		OutInfo.Category = Property->GetMetaData(TEXT("Category"));
	}
	if (Property->HasMetaData(TEXT("Tooltip")))
	{
		OutInfo.Tooltip = Property->GetMetaData(TEXT("Tooltip"));
	}
}

bool FWidgetPropertyService::ExtractPropertyValue(FProperty* Property, void* Container, FString& OutValue)
{
	if (!Property || !Container)
	{
		return false;
	}

	if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
	{
		OutValue = StrProperty->GetPropertyValue_InContainer(Container);
		return true;
	}
	else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		FText TextValue = TextProperty->GetPropertyValue_InContainer(Container);
		OutValue = TextValue.ToString();
		return true;
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		bool BoolValue = BoolProperty->GetPropertyValue_InContainer(Container);
		OutValue = BoolValue ? TEXT("true") : TEXT("false");
		return true;
	}
	else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
	{
		float FloatValue = FloatProperty->GetPropertyValue_InContainer(Container);
		OutValue = FString::SanitizeFloat(FloatValue);
		return true;
	}
	else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
	{
		int32 IntValue = IntProperty->GetPropertyValue_InContainer(Container);
		OutValue = FString::FromInt(IntValue);
		return true;
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		uint8 ByteValue = ByteProperty->GetPropertyValue_InContainer(Container);
		if (ByteProperty->Enum)
		{
			OutValue = ByteProperty->Enum->GetNameStringByValue(ByteValue);
		}
		else
		{
			OutValue = FString::FromInt(ByteValue);
		}
		return true;
	}
	else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		if (EnumProperty->GetUnderlyingProperty() && EnumProperty->GetEnum())
		{
			uint8* EnumValuePtr = (uint8*)EnumProperty->ContainerPtrToValuePtr<void>(Container);
			int64 EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumValuePtr);
			OutValue = EnumProperty->GetEnum()->GetNameStringByValue(EnumValue);
			return true;
		}
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		OutValue = TEXT("ComplexType");
		return true;
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		OutValue = TEXT("Array");
		return true;
	}

	OutValue = TEXT("UnknownType");
	return false;
}

bool FWidgetPropertyService::SetPropertyValue(FProperty* Property, void* Container, const FString& Value)
{
	if (!Property || !Container)
	{
		return false;
	}

	if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
	{
		StrProperty->SetPropertyValue_InContainer(Container, Value);
		return true;
	}
	else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		FText TextValue = FText::FromString(Value);
		TextProperty->SetPropertyValue_InContainer(Container, TextValue);
		return true;
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		bool BoolValue = Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("1"));
		BoolProperty->SetPropertyValue_InContainer(Container, BoolValue);
		return true;
	}
	else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
	{
		float FloatValue = FCString::Atof(*Value);
		FloatProperty->SetPropertyValue_InContainer(Container, FloatValue);
		return true;
	}
	else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
	{
		int32 IntValue = FCString::Atoi(*Value);
		IntProperty->SetPropertyValue_InContainer(Container, IntValue);
		return true;
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		if (ByteProperty->Enum)
		{
			int64 EnumValue = ByteProperty->Enum->GetValueByNameString(Value);
			if (EnumValue != INDEX_NONE)
			{
				ByteProperty->SetPropertyValue_InContainer(Container, (uint8)EnumValue);
				return true;
			}
			return false;
		}
		else
		{
			uint8 ByteValue = (uint8)FCString::Atoi(*Value);
			ByteProperty->SetPropertyValue_InContainer(Container, ByteValue);
			return true;
		}
	}
	else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		if (EnumProperty->GetUnderlyingProperty() && EnumProperty->GetEnum())
		{
			int64 EnumValue = EnumProperty->GetEnum()->GetValueByNameString(Value);
			if (EnumValue != INDEX_NONE)
			{
				uint8* EnumValuePtr = (uint8*)EnumProperty->ContainerPtrToValuePtr<void>(Container);
				EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(EnumValuePtr, EnumValue);
				return true;
			}
			return false;
		}
	}

	// Unsupported type
	return false;
}
