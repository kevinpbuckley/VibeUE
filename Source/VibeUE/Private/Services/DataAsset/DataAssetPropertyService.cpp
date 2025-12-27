// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Services/DataAsset/DataAssetPropertyService.h"
#include "Core/ServiceContext.h"
#include "Core/ErrorCodes.h"
#include "Engine/DataAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataAssetPropertyService, Log, All);

FDataAssetPropertyService::FDataAssetPropertyService(TSharedPtr<FServiceContext> InServiceContext)
	: ServiceContext(InServiceContext)
{
}

// ========== Asset Information ==========

TResult<TSharedPtr<FJsonObject>> FDataAssetPropertyService::GetAssetInfo(UDataAsset* DataAsset)
{
	if (!DataAsset)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("DataAsset is required"));
	}
	
	UClass* AssetClass = DataAsset->GetClass();
	
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), DataAsset->GetName());
	Result->SetStringField(TEXT("path"), DataAsset->GetPathName());
	Result->SetStringField(TEXT("class"), AssetClass->GetName());
	Result->SetStringField(TEXT("class_path"), AssetClass->GetPathName());
	
	// Get parent class chain
	TArray<TSharedPtr<FJsonValue>> ParentChain;
	UClass* CurrentClass = AssetClass->GetSuperClass();
	while (CurrentClass && CurrentClass != UObject::StaticClass())
	{
		ParentChain.Add(MakeShared<FJsonValueString>(CurrentClass->GetName()));
		CurrentClass = CurrentClass->GetSuperClass();
	}
	Result->SetArrayField(TEXT("parent_classes"), ParentChain);
	
	// Get all properties with values
	TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
	
	for (TFieldIterator<FProperty> PropIt(AssetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!ShouldExposeProperty(Property))
		{
			continue;
		}
		
		TSharedPtr<FJsonValue> Value = PropertyToJson(Property, DataAsset);
		if (Value.IsValid())
		{
			PropertiesObj->SetField(Property->GetName(), Value);
		}
	}
	
	Result->SetObjectField(TEXT("properties"), PropertiesObj);
	
	return TResult<TSharedPtr<FJsonObject>>::Success(Result);
}

TResult<TSharedPtr<FJsonObject>> FDataAssetPropertyService::GetClassInfo(UClass* AssetClass, bool bIncludeAll)
{
	if (!AssetClass)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("AssetClass is required"));
	}
	
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), AssetClass->GetName());
	Result->SetStringField(TEXT("path"), AssetClass->GetPathName());
	Result->SetBoolField(TEXT("is_abstract"), AssetClass->HasAnyClassFlags(CLASS_Abstract));
	Result->SetBoolField(TEXT("is_native"), !AssetClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint));
	
	// Parent chain
	TArray<TSharedPtr<FJsonValue>> ParentChain;
	UClass* CurrentClass = AssetClass->GetSuperClass();
	while (CurrentClass && CurrentClass != UObject::StaticClass())
	{
		ParentChain.Add(MakeShared<FJsonValueString>(CurrentClass->GetName()));
		CurrentClass = CurrentClass->GetSuperClass();
	}
	Result->SetArrayField(TEXT("parent_classes"), ParentChain);
	
	// Properties
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (TFieldIterator<FProperty> PropIt(AssetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!ShouldExposeProperty(Property, bIncludeAll))
		{
			continue;
		}
		
		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Property->GetName());
		PropObj->SetStringField(TEXT("type"), GetPropertyTypeString(Property));
		PropObj->SetStringField(TEXT("defined_in"), Property->GetOwnerClass()->GetName());
		
		if (bIncludeAll)
		{
			TArray<FString> Flags;
			if (Property->HasAnyPropertyFlags(CPF_Edit)) Flags.Add(TEXT("Edit"));
			if (Property->HasAnyPropertyFlags(CPF_BlueprintVisible)) Flags.Add(TEXT("BlueprintVisible"));
			if (Property->HasAnyPropertyFlags(CPF_SaveGame)) Flags.Add(TEXT("SaveGame"));
			if (Property->HasAnyPropertyFlags(CPF_EditConst)) Flags.Add(TEXT("EditConst"));
			if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate)) Flags.Add(TEXT("Private"));
			if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierProtected)) Flags.Add(TEXT("Protected"));
			if (Property->HasAnyPropertyFlags(CPF_Transient)) Flags.Add(TEXT("Transient"));
			PropObj->SetStringField(TEXT("flags"), FString::Join(Flags, TEXT(", ")));
		}
		
		PropertiesArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}
	Result->SetArrayField(TEXT("properties"), PropertiesArray);
	
	if (bIncludeAll && PropertiesArray.Num() == 0)
	{
		Result->SetStringField(TEXT("note"), TEXT("This class has no properties at all. It may use custom serialization or internal data structures not exposed via UPROPERTY."));
	}
	
	return TResult<TSharedPtr<FJsonObject>>::Success(Result);
}

// ========== Property Listing ==========

TResult<TArray<FDataAssetPropertyInfo>> FDataAssetPropertyService::ListProperties(UClass* AssetClass, bool bIncludeAll)
{
	if (!AssetClass)
	{
		return TResult<TArray<FDataAssetPropertyInfo>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("AssetClass is required"));
	}
	
	TArray<FDataAssetPropertyInfo> Properties;
	
	for (TFieldIterator<FProperty> PropIt(AssetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!ShouldExposeProperty(Property, bIncludeAll))
		{
			continue;
		}
		
		FDataAssetPropertyInfo PropInfo;
		PropInfo.Name = Property->GetName();
		PropInfo.Type = GetPropertyTypeString(Property);
		PropInfo.Category = Property->GetMetaData(TEXT("Category"));
		PropInfo.Description = Property->GetMetaData(TEXT("ToolTip"));
		PropInfo.DefinedIn = Property->GetOwnerClass()->GetName();
		PropInfo.bReadOnly = Property->HasAnyPropertyFlags(CPF_EditConst);
		PropInfo.bIsArray = Property->IsA<FArrayProperty>();
		
		if (bIncludeAll)
		{
			TArray<FString> Flags;
			if (Property->HasAnyPropertyFlags(CPF_Edit)) Flags.Add(TEXT("Edit"));
			if (Property->HasAnyPropertyFlags(CPF_BlueprintVisible)) Flags.Add(TEXT("BlueprintVisible"));
			if (Property->HasAnyPropertyFlags(CPF_SaveGame)) Flags.Add(TEXT("SaveGame"));
			if (Property->HasAnyPropertyFlags(CPF_EditConst)) Flags.Add(TEXT("EditConst"));
			if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate)) Flags.Add(TEXT("Private"));
			if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierProtected)) Flags.Add(TEXT("Protected"));
			if (Property->HasAnyPropertyFlags(CPF_Transient)) Flags.Add(TEXT("Transient"));
			PropInfo.Flags = FString::Join(Flags, TEXT(", "));
		}
		
		Properties.Add(MoveTemp(PropInfo));
	}
	
	return TResult<TArray<FDataAssetPropertyInfo>>::Success(MoveTemp(Properties));
}

// ========== Property Access ==========

TResult<TSharedPtr<FJsonValue>> FDataAssetPropertyService::GetProperty(UDataAsset* DataAsset, const FString& PropertyName)
{
	if (!DataAsset)
	{
		return TResult<TSharedPtr<FJsonValue>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("DataAsset is required"));
	}
	
	UClass* AssetClass = DataAsset->GetClass();
	FProperty* Property = AssetClass->FindPropertyByName(*PropertyName);
	
	if (!Property)
	{
		return TResult<TSharedPtr<FJsonValue>>::Error(
			VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
			FString::Printf(TEXT("Property not found: %s"), *PropertyName));
	}
	
	TSharedPtr<FJsonValue> Value = PropertyToJson(Property, DataAsset);
	return TResult<TSharedPtr<FJsonValue>>::Success(Value);
}

TResult<void> FDataAssetPropertyService::SetProperty(UDataAsset* DataAsset, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value)
{
	if (!DataAsset)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("DataAsset is required"));
	}
	
	if (!Value.IsValid())
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Value is required"));
	}
	
	UClass* AssetClass = DataAsset->GetClass();
	FProperty* Property = AssetClass->FindPropertyByName(*PropertyName);
	
	if (!Property)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
			FString::Printf(TEXT("Property not found: %s"), *PropertyName));
	}
	
	if (!ShouldExposeProperty(Property))
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::PROPERTY_READ_ONLY,
			FString::Printf(TEXT("Property is not editable: %s"), *PropertyName));
	}
	
	FString Error;
	if (!JsonToProperty(Property, DataAsset, Value, Error))
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
			FString::Printf(TEXT("Failed to set property: %s"), *Error));
	}
	
	DataAsset->MarkPackageDirty();
	return TResult<void>::Success();
}

TResult<FSetPropertiesResult> FDataAssetPropertyService::SetProperties(UDataAsset* DataAsset, const TSharedPtr<FJsonObject>& Properties)
{
	if (!DataAsset)
	{
		return TResult<FSetPropertiesResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("DataAsset is required"));
	}
	
	if (!Properties.IsValid())
	{
		return TResult<FSetPropertiesResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Properties object is required"));
	}
	
	UClass* AssetClass = DataAsset->GetClass();
	FSetPropertiesResult Result;
	
	for (auto& Pair : Properties->Values)
	{
		FProperty* Property = AssetClass->FindPropertyByName(*Pair.Key);
		if (!Property)
		{
			Result.FailedProperties.Add(FString::Printf(TEXT("%s: not found"), *Pair.Key));
			continue;
		}
		
		if (!ShouldExposeProperty(Property))
		{
			Result.FailedProperties.Add(FString::Printf(TEXT("%s: not editable"), *Pair.Key));
			continue;
		}
		
		FString Error;
		if (JsonToProperty(Property, DataAsset, Pair.Value, Error))
		{
			Result.SuccessProperties.Add(Pair.Key);
		}
		else
		{
			Result.FailedProperties.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Error));
		}
	}
	
	DataAsset->MarkPackageDirty();
	return TResult<FSetPropertiesResult>::Success(MoveTemp(Result));
}

// ========== Serialization Helpers ==========

TSharedPtr<FJsonValue> FDataAssetPropertyService::PropertyToJson(FProperty* Property, void* Container)
{
	if (!Property || !Container)
	{
		return MakeShared<FJsonValueNull>();
	}
	
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	
	// Numeric types
	if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
	{
		if (NumericProp->IsFloatingPoint())
		{
			double Value = 0.0;
			NumericProp->GetValue_InContainer(Container, &Value);
			return MakeShared<FJsonValueNumber>(Value);
		}
		else if (NumericProp->IsInteger())
		{
			int64 Value = 0;
			NumericProp->GetValue_InContainer(Container, &Value);
			return MakeShared<FJsonValueNumber>(static_cast<double>(Value));
		}
	}
	
	// Bool
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
	}
	
	// String types
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
	}
	
	if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
	}
	
	if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
	}
	
	// Enum
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		UEnum* Enum = EnumProp->GetEnum();
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
		FString EnumName = Enum->GetNameStringByValue(EnumValue);
		return MakeShared<FJsonValueString>(EnumName);
	}
	
	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (UEnum* Enum = ByteProp->Enum)
		{
			uint8 Value = ByteProp->GetPropertyValue(ValuePtr);
			FString EnumName = Enum->GetNameStringByValue(Value);
			return MakeShared<FJsonValueString>(EnumName);
		}
		else
		{
			return MakeShared<FJsonValueNumber>(ByteProp->GetPropertyValue(ValuePtr));
		}
	}
	
	// Object reference
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
		if (Obj)
		{
			return MakeShared<FJsonValueString>(Obj->GetPathName());
		}
		return MakeShared<FJsonValueNull>();
	}
	
	// Soft object reference
	if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		FSoftObjectPtr* SoftPtr = static_cast<FSoftObjectPtr*>(ValuePtr);
		return MakeShared<FJsonValueString>(SoftPtr->ToString());
	}
	
	// Array
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> JsonArray;
		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
		
		for (int32 i = 0; i < ArrayHelper.Num(); ++i)
		{
			void* ElementPtr = ArrayHelper.GetRawPtr(i);
			TSharedPtr<FJsonValue> ElementValue = PropertyToJson(ArrayProp->Inner, ElementPtr);
			JsonArray.Add(ElementValue);
		}
		
		return MakeShared<FJsonValueArray>(JsonArray);
	}
	
	// Struct
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		TSharedPtr<FJsonObject> StructObj = MakeShared<FJsonObject>();
		UScriptStruct* Struct = StructProp->Struct;
		
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* InnerProp = *It;
			TSharedPtr<FJsonValue> InnerValue = PropertyToJson(InnerProp, ValuePtr);
			StructObj->SetField(InnerProp->GetName(), InnerValue);
		}
		
		return MakeShared<FJsonValueObject>(StructObj);
	}
	
	// Map
	if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
		FScriptMapHelper MapHelper(MapProp, ValuePtr);
		
		for (int32 i = 0; i < MapHelper.Num(); ++i)
		{
			if (MapHelper.IsValidIndex(i))
			{
				void* KeyPtr = MapHelper.GetKeyPtr(i);
				void* ValPtr = MapHelper.GetValuePtr(i);
				
				FString KeyStr;
				MapProp->KeyProp->ExportTextItem_Direct(KeyStr, KeyPtr, nullptr, nullptr, PPF_None);
				
				TSharedPtr<FJsonValue> Value = PropertyToJson(MapProp->ValueProp, ValPtr);
				MapObj->SetField(KeyStr, Value);
			}
		}
		
		return MakeShared<FJsonValueObject>(MapObj);
	}
	
	// Fallback: export as text
	FString ExportedText;
	Property->ExportTextItem_Direct(ExportedText, ValuePtr, nullptr, nullptr, PPF_None);
	return MakeShared<FJsonValueString>(ExportedText);
}

bool FDataAssetPropertyService::JsonToProperty(FProperty* Property, void* Container, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	if (!Property || !Container || !Value.IsValid())
	{
		OutError = TEXT("Invalid parameters");
		return false;
	}
	
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	
	// Numeric types
	if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
	{
		double NumValue;
		if (!Value->TryGetNumber(NumValue))
		{
			OutError = TEXT("Expected numeric value");
			return false;
		}
		
		if (NumericProp->IsFloatingPoint())
		{
			NumericProp->SetFloatingPointPropertyValue(ValuePtr, NumValue);
		}
		else
		{
			NumericProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumValue));
		}
		return true;
	}
	
	// Bool
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool BoolValue;
		if (!Value->TryGetBool(BoolValue))
		{
			OutError = TEXT("Expected boolean value");
			return false;
		}
		BoolProp->SetPropertyValue(ValuePtr, BoolValue);
		return true;
	}
	
	// String types
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString StrValue;
		if (!Value->TryGetString(StrValue))
		{
			OutError = TEXT("Expected string value");
			return false;
		}
		StrProp->SetPropertyValue(ValuePtr, StrValue);
		return true;
	}
	
	if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString StrValue;
		if (!Value->TryGetString(StrValue))
		{
			OutError = TEXT("Expected string value for FName");
			return false;
		}
		NameProp->SetPropertyValue(ValuePtr, FName(*StrValue));
		return true;
	}
	
	if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		FString StrValue;
		if (!Value->TryGetString(StrValue))
		{
			OutError = TEXT("Expected string value for FText");
			return false;
		}
		TextProp->SetPropertyValue(ValuePtr, FText::FromString(StrValue));
		return true;
	}
	
	// Enum
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		FString EnumStr;
		if (Value->TryGetString(EnumStr))
		{
			UEnum* Enum = EnumProp->GetEnum();
			int64 EnumValue = Enum->GetValueByNameString(EnumStr);
			if (EnumValue == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("Invalid enum value: %s"), *EnumStr);
				return false;
			}
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumValue);
			return true;
		}
		
		double NumValue;
		if (Value->TryGetNumber(NumValue))
		{
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumValue));
			return true;
		}
		
		OutError = TEXT("Expected string or number for enum");
		return false;
	}
	
	// Object reference (as path string)
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		FString PathStr;
		if (Value->IsNull())
		{
			ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
			return true;
		}
		
		if (!Value->TryGetString(PathStr))
		{
			OutError = TEXT("Expected string path for object reference");
			return false;
		}
		
		UObject* Obj = StaticLoadObject(ObjProp->PropertyClass, nullptr, *PathStr);
		if (!Obj && !PathStr.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Could not load object: %s"), *PathStr);
			return false;
		}
		
		ObjProp->SetObjectPropertyValue(ValuePtr, Obj);
		return true;
	}
	
	// Soft object reference
	if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		FString PathStr;
		if (!Value->TryGetString(PathStr))
		{
			OutError = TEXT("Expected string path for soft object reference");
			return false;
		}
		
		FSoftObjectPtr* SoftPtr = static_cast<FSoftObjectPtr*>(ValuePtr);
		*SoftPtr = FSoftObjectPath(PathStr);
		return true;
	}
	
	// Array
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		const TArray<TSharedPtr<FJsonValue>>* JsonArray;
		if (!Value->TryGetArray(JsonArray))
		{
			OutError = TEXT("Expected array value");
			return false;
		}
		
		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
		ArrayHelper.EmptyValues();
		ArrayHelper.AddValues(JsonArray->Num());
		
		for (int32 i = 0; i < JsonArray->Num(); ++i)
		{
			FString ElementError;
			void* ElementPtr = ArrayHelper.GetRawPtr(i);
			
			if (!JsonToProperty(ArrayProp->Inner, ElementPtr, (*JsonArray)[i], ElementError))
			{
				OutError = FString::Printf(TEXT("Array element %d: %s"), i, *ElementError);
				return false;
			}
		}
		return true;
	}
	
	// Struct - try object format first, then string import
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		const TSharedPtr<FJsonObject>* JsonObj;
		if (Value->TryGetObject(JsonObj))
		{
			UScriptStruct* Struct = StructProp->Struct;
			
			for (auto& Pair : (*JsonObj)->Values)
			{
				FProperty* InnerProp = Struct->FindPropertyByName(*Pair.Key);
				if (InnerProp)
				{
					FString InnerError;
					if (!JsonToProperty(InnerProp, ValuePtr, Pair.Value, InnerError))
					{
						UE_LOG(LogDataAssetPropertyService, Warning, TEXT("Failed to set struct member %s: %s"), *Pair.Key, *InnerError);
					}
				}
			}
			return true;
		}
		
		// Try string import
		FString StrValue;
		if (Value->TryGetString(StrValue))
		{
			if (StructProp->ImportText_Direct(*StrValue, ValuePtr, nullptr, PPF_None))
			{
				return true;
			}
			OutError = FString::Printf(TEXT("Failed to import struct from string: %s"), *StrValue);
			return false;
		}
		
		OutError = TEXT("Expected object or string for struct");
		return false;
	}
	
	// Fallback: try ImportText
	FString StrValue;
	if (Value->TryGetString(StrValue))
	{
		if (Property->ImportText_Direct(*StrValue, ValuePtr, nullptr, PPF_None))
		{
			return true;
		}
	}
	
	OutError = TEXT("Could not convert JSON value to property");
	return false;
}

FString FDataAssetPropertyService::GetPropertyTypeString(FProperty* Property)
{
	if (!Property)
	{
		return TEXT("Unknown");
	}
	
	if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
	{
		if (NumericProp->IsFloatingPoint())
		{
			if (CastField<FFloatProperty>(Property)) return TEXT("float");
			if (CastField<FDoubleProperty>(Property)) return TEXT("double");
		}
		else
		{
			if (CastField<FIntProperty>(Property)) return TEXT("int32");
			if (CastField<FInt64Property>(Property)) return TEXT("int64");
			if (CastField<FUInt32Property>(Property)) return TEXT("uint32");
			if (CastField<FUInt64Property>(Property)) return TEXT("uint64");
			if (CastField<FInt16Property>(Property)) return TEXT("int16");
			if (CastField<FUInt16Property>(Property)) return TEXT("uint16");
			if (CastField<FInt8Property>(Property)) return TEXT("int8");
		}
		return TEXT("numeric");
	}
	
	if (CastField<FBoolProperty>(Property)) return TEXT("bool");
	if (CastField<FStrProperty>(Property)) return TEXT("FString");
	if (CastField<FNameProperty>(Property)) return TEXT("FName");
	if (CastField<FTextProperty>(Property)) return TEXT("FText");
	
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		if (UEnum* Enum = EnumProp->GetEnum())
		{
			return Enum->GetName();
		}
		return TEXT("Enum");
	}
	
	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (UEnum* Enum = ByteProp->Enum)
		{
			return Enum->GetName();
		}
		return TEXT("uint8");
	}
	
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		return FString::Printf(TEXT("%s*"), *ObjProp->PropertyClass->GetName());
	}
	
	if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *SoftObjProp->PropertyClass->GetName());
	}
	
	if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		return FString::Printf(TEXT("TSubclassOf<%s>"), *ClassProp->MetaClass->GetName());
	}
	
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		return FString::Printf(TEXT("TArray<%s>"), *GetPropertyTypeString(ArrayProp->Inner));
	}
	
	if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
	{
		return FString::Printf(TEXT("TSet<%s>"), *GetPropertyTypeString(SetProp->ElementProp));
	}
	
	if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		return FString::Printf(TEXT("TMap<%s, %s>"), 
			*GetPropertyTypeString(MapProp->KeyProp),
			*GetPropertyTypeString(MapProp->ValueProp));
	}
	
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		return StructProp->Struct->GetName();
	}
	
	return Property->GetCPPType();
}

bool FDataAssetPropertyService::ShouldExposeProperty(FProperty* Property, bool bIncludeAll)
{
	if (!Property)
	{
		return false;
	}
	
	// If including all, just skip null and deprecated
	if (bIncludeAll)
	{
		if (Property->HasMetaData(TEXT("DeprecatedProperty")))
		{
			return false;
		}
		return true;
	}
	
	// Must be editable in some way - check this FIRST
	bool bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible | CPF_SaveGame);
	
	// If property is marked as editable (Edit flag), allow it even if it's private
	if (!bIsEditable)
	{
		if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected))
		{
			return false;
		}
		return false;
	}
	
	// Skip deprecated
	if (Property->HasMetaData(TEXT("DeprecatedProperty")))
	{
		return false;
	}
	
	return true;
}
