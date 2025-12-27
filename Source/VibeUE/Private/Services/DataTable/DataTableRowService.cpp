// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Services/DataTable/DataTableRowService.h"
#include "Core/ErrorCodes.h"
#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataTableRow, Log, All);

FDataTableRowService::FDataTableRowService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

// ========== Row Queries ==========

TResult<TArray<FName>> FDataTableRowService::ListRowNames(UDataTable* DataTable)
{
	if (!DataTable)
	{
		return TResult<TArray<FName>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Data table is required"));
	}
	
	return TResult<TArray<FName>>::Success(DataTable->GetRowNames());
}

TResult<TSharedPtr<FJsonObject>> FDataTableRowService::GetRow(UDataTable* DataTable, FName RowName)
{
	if (!DataTable)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Data table is required"));
	}
	
	if (RowName.IsNone())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Row name is required"));
	}
	
	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::INTERNAL_ERROR,
			TEXT("Data table has no row struct"));
	}
	
	// Find the row
	void* RowData = DataTable->FindRowUnchecked(RowName);
	if (!RowData)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::ROW_NOT_FOUND,
			FString::Printf(TEXT("Row '%s' not found in table"), *RowName.ToString()));
	}
	
	// Serialize to JSON
	TSharedPtr<FJsonObject> JsonObj = RowToJson(RowStruct, RowData);
	
	return TResult<TSharedPtr<FJsonObject>>::Success(JsonObj);
}

TResult<TSharedPtr<FJsonObject>> FDataTableRowService::GetAllRows(UDataTable* DataTable, int32 MaxRows)
{
	if (!DataTable)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Data table is required"));
	}
	
	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::INTERNAL_ERROR,
			TEXT("Data table has no row struct"));
	}
	
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	TArray<FName> RowNames = DataTable->GetRowNames();
	
	int32 Count = 0;
	for (const FName& RowName : RowNames)
	{
		if (MaxRows > 0 && Count >= MaxRows)
		{
			break;
		}
		
		void* RowData = DataTable->FindRowUnchecked(RowName);
		if (RowData)
		{
			TSharedPtr<FJsonObject> RowJson = RowToJson(RowStruct, RowData);
			Result->SetObjectField(RowName.ToString(), RowJson);
			Count++;
		}
	}
	
	return TResult<TSharedPtr<FJsonObject>>::Success(Result);
}

// ========== Row Mutations ==========

TResult<FRowOperationResult> FDataTableRowService::AddRow(
	UDataTable* DataTable,
	FName RowName,
	const TSharedPtr<FJsonObject>& RowData)
{
	if (!DataTable)
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Data table is required"));
	}
	
	if (RowName.IsNone())
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Row name is required"));
	}
	
	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::INTERNAL_ERROR,
			TEXT("Data table has no row struct"));
	}
	
	// Check if row already exists
	if (DataTable->FindRowUnchecked(RowName))
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::ROW_ALREADY_EXISTS,
			FString::Printf(TEXT("Row '%s' already exists. Use update_row to modify it."), *RowName.ToString()));
	}
	
	// Allocate memory for new row and initialize with defaults
	void* NewRowData = FMemory::Malloc(RowStruct->GetStructureSize());
	RowStruct->InitializeStruct(NewRowData);
	
	// Apply provided data if any
	FRowOperationResult OpResult;
	OpResult.RowName = RowName.ToString();
	
	if (RowData.IsValid())
	{
		FString Error;
		if (!JsonToRow(RowStruct, NewRowData, RowData, Error))
		{
			RowStruct->DestroyStruct(NewRowData);
			FMemory::Free(NewRowData);
			
			return TResult<FRowOperationResult>::Error(
				VibeUE::ErrorCodes::PARAM_INVALID,
				FString::Printf(TEXT("Failed to apply row data: %s"), *Error));
		}
		
		// Track modified properties
		for (auto& Pair : RowData->Values)
		{
			OpResult.ModifiedProperties.Add(Pair.Key);
		}
	}
	
	// Add row to table
	DataTable->AddRow(RowName, *static_cast<FTableRowBase*>(NewRowData));
	
	// Cleanup temporary row data
	RowStruct->DestroyStruct(NewRowData);
	FMemory::Free(NewRowData);
	
	// Mark table dirty
	DataTable->MarkPackageDirty();
	
	OpResult.bSuccess = true;
	return TResult<FRowOperationResult>::Success(MoveTemp(OpResult));
}

TResult<FRowOperationResult> FDataTableRowService::UpdateRow(
	UDataTable* DataTable,
	FName RowName,
	const TSharedPtr<FJsonObject>& RowData)
{
	if (!DataTable)
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Data table is required"));
	}
	
	if (RowName.IsNone())
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Row name is required"));
	}
	
	if (!RowData.IsValid() || RowData->Values.Num() == 0)
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Row data is required for update"));
	}
	
	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::INTERNAL_ERROR,
			TEXT("Data table has no row struct"));
	}
	
	// Find existing row
	void* ExistingRowData = DataTable->FindRowUnchecked(RowName);
	if (!ExistingRowData)
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::ROW_NOT_FOUND,
			FString::Printf(TEXT("Row '%s' not found. Use add_row to create it."), *RowName.ToString()));
	}
	
	// Apply updates (partial update)
	FRowOperationResult OpResult;
	OpResult.RowName = RowName.ToString();
	
	for (auto& Pair : RowData->Values)
	{
		FProperty* Property = RowStruct->FindPropertyByName(*Pair.Key);
		if (!Property)
		{
			UE_LOG(LogDataTableRow, Warning, TEXT("Property '%s' not found in row struct"), *Pair.Key);
			continue;
		}
		
		FString Error;
		if (JsonToProperty(Property, ExistingRowData, Pair.Value, Error))
		{
			OpResult.ModifiedProperties.Add(Pair.Key);
		}
		else
		{
			UE_LOG(LogDataTableRow, Warning, TEXT("Failed to set property '%s': %s"), *Pair.Key, *Error);
		}
	}
	
	// Mark table dirty
	DataTable->MarkPackageDirty();
	
	OpResult.bSuccess = true;
	return TResult<FRowOperationResult>::Success(MoveTemp(OpResult));
}

TResult<FRowOperationResult> FDataTableRowService::RemoveRow(UDataTable* DataTable, FName RowName)
{
	if (!DataTable)
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Data table is required"));
	}
	
	if (RowName.IsNone())
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Row name is required"));
	}
	
	// Check if row exists
	if (!DataTable->FindRowUnchecked(RowName))
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::ROW_NOT_FOUND,
			FString::Printf(TEXT("Row '%s' not found"), *RowName.ToString()));
	}
	
	// Remove the row
	DataTable->RemoveRow(RowName);
	
	// Mark table dirty
	DataTable->MarkPackageDirty();
	
	FRowOperationResult OpResult;
	OpResult.RowName = RowName.ToString();
	OpResult.bSuccess = true;
	return TResult<FRowOperationResult>::Success(MoveTemp(OpResult));
}

TResult<FRowOperationResult> FDataTableRowService::RenameRow(UDataTable* DataTable, FName OldName, FName NewName)
{
	if (!DataTable)
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Data table is required"));
	}
	
	if (OldName.IsNone() || NewName.IsNone())
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Both old and new row names are required"));
	}
	
	if (OldName == NewName)
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Old and new names are the same"));
	}
	
	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::INTERNAL_ERROR,
			TEXT("Data table has no row struct"));
	}
	
	// Check if old row exists
	void* OldRowData = DataTable->FindRowUnchecked(OldName);
	if (!OldRowData)
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::ROW_NOT_FOUND,
			FString::Printf(TEXT("Row '%s' not found"), *OldName.ToString()));
	}
	
	// Check if new name already exists
	if (DataTable->FindRowUnchecked(NewName))
	{
		return TResult<FRowOperationResult>::Error(
			VibeUE::ErrorCodes::ROW_ALREADY_EXISTS,
			FString::Printf(TEXT("Row '%s' already exists"), *NewName.ToString()));
	}
	
	// Copy the row data
	void* CopiedRowData = FMemory::Malloc(RowStruct->GetStructureSize());
	RowStruct->InitializeStruct(CopiedRowData);
	RowStruct->CopyScriptStruct(CopiedRowData, OldRowData);
	
	// Add new row
	DataTable->AddRow(NewName, *static_cast<FTableRowBase*>(CopiedRowData));
	
	// Remove old row
	DataTable->RemoveRow(OldName);
	
	// Cleanup
	RowStruct->DestroyStruct(CopiedRowData);
	FMemory::Free(CopiedRowData);
	
	// Mark table dirty
	DataTable->MarkPackageDirty();
	
	FRowOperationResult OpResult;
	OpResult.RowName = NewName.ToString();
	OpResult.bSuccess = true;
	return TResult<FRowOperationResult>::Success(MoveTemp(OpResult));
}

// ========== Bulk Operations ==========

TResult<FBulkRowResult> FDataTableRowService::AddRows(
	UDataTable* DataTable,
	const TMap<FName, TSharedPtr<FJsonObject>>& Rows)
{
	if (!DataTable)
	{
		return TResult<FBulkRowResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Data table is required"));
	}
	
	FBulkRowResult Result;
	Result.TotalCount = Rows.Num();
	
	for (const auto& Pair : Rows)
	{
		auto AddResult = AddRow(DataTable, Pair.Key, Pair.Value);
		if (AddResult.IsSuccess())
		{
			Result.SucceededRows.Add(Pair.Key.ToString());
		}
		else
		{
			Result.FailedRows.Add(Pair.Key.ToString(), AddResult.GetErrorMessage());
		}
	}
	
	return TResult<FBulkRowResult>::Success(MoveTemp(Result));
}

TResult<int32> FDataTableRowService::ClearRows(UDataTable* DataTable)
{
	if (!DataTable)
	{
		return TResult<int32>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Data table is required"));
	}
	
	int32 RowCount = DataTable->GetRowNames().Num();
	
	// Empty the table
	DataTable->EmptyTable();
	
	// Mark table dirty
	DataTable->MarkPackageDirty();
	
	return TResult<int32>::Success(RowCount);
}

TResult<FBulkRowResult> FDataTableRowService::ImportFromJson(
	UDataTable* DataTable,
	const TSharedPtr<FJsonObject>& JsonData,
	bool bReplace)
{
	if (!DataTable)
	{
		return TResult<FBulkRowResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Data table is required"));
	}
	
	if (!JsonData.IsValid())
	{
		return TResult<FBulkRowResult>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("JSON data is required"));
	}
	
	// Clear if replace mode
	if (bReplace)
	{
		DataTable->EmptyTable();
	}
	
	FBulkRowResult Result;
	Result.TotalCount = JsonData->Values.Num();
	
	for (const auto& Pair : JsonData->Values)
	{
		FName RowName(*Pair.Key);
		
		const TSharedPtr<FJsonObject>* RowObj;
		if (!Pair.Value->TryGetObject(RowObj) || !RowObj->IsValid())
		{
			Result.FailedRows.Add(Pair.Key, TEXT("Invalid row data - expected object"));
			continue;
		}
		
		// Check if row exists
		bool bRowExists = DataTable->FindRowUnchecked(RowName) != nullptr;
		
		if (bRowExists)
		{
			// Update existing row
			auto UpdateResult = UpdateRow(DataTable, RowName, *RowObj);
			if (UpdateResult.IsSuccess())
			{
				Result.SucceededRows.Add(Pair.Key);
			}
			else
			{
				Result.FailedRows.Add(Pair.Key, UpdateResult.GetErrorMessage());
			}
		}
		else
		{
			// Add new row
			auto AddResult = AddRow(DataTable, RowName, *RowObj);
			if (AddResult.IsSuccess())
			{
				Result.SucceededRows.Add(Pair.Key);
			}
			else
			{
				Result.FailedRows.Add(Pair.Key, AddResult.GetErrorMessage());
			}
		}
	}
	
	return TResult<FBulkRowResult>::Success(MoveTemp(Result));
}

// ========== Serialization Helpers ==========

TSharedPtr<FJsonObject> FDataTableRowService::RowToJson(const UScriptStruct* RowStruct, void* RowData)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	
	if (!RowStruct || !RowData)
	{
		return JsonObj;
	}
	
	for (TFieldIterator<FProperty> PropIt(RowStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		TSharedPtr<FJsonValue> Value = PropertyToJson(Property, RowData);
		JsonObj->SetField(Property->GetName(), Value);
	}
	
	return JsonObj;
}

bool FDataTableRowService::JsonToRow(
	const UScriptStruct* RowStruct,
	void* RowData,
	const TSharedPtr<FJsonObject>& JsonObj,
	FString& OutError)
{
	if (!RowStruct || !RowData || !JsonObj.IsValid())
	{
		OutError = TEXT("Invalid parameters");
		return false;
	}
	
	bool bAllSuccess = true;
	
	for (const auto& Pair : JsonObj->Values)
	{
		FProperty* Property = RowStruct->FindPropertyByName(*Pair.Key);
		if (!Property)
		{
			UE_LOG(LogDataTableRow, Warning, TEXT("Property '%s' not found in row struct"), *Pair.Key);
			continue;
		}
		
		FString PropertyError;
		if (!JsonToProperty(Property, RowData, Pair.Value, PropertyError))
		{
			UE_LOG(LogDataTableRow, Warning, TEXT("Failed to set property '%s': %s"), *Pair.Key, *PropertyError);
			if (bAllSuccess)
			{
				OutError = FString::Printf(TEXT("Failed to set '%s': %s"), *Pair.Key, *PropertyError);
				bAllSuccess = false;
			}
		}
	}
	
	return bAllSuccess;
}

TSharedPtr<FJsonValue> FDataTableRowService::PropertyToJson(FProperty* Property, void* Container)
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
		else
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

bool FDataTableRowService::JsonToProperty(
	FProperty* Property,
	void* Container,
	const TSharedPtr<FJsonValue>& Value,
	FString& OutError)
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
	
	// Object reference
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		if (Value->IsNull())
		{
			ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
			return true;
		}
		
		FString PathStr;
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
	
	// Struct
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
						UE_LOG(LogDataTableRow, Warning, TEXT("Failed to set struct member %s: %s"), *Pair.Key, *InnerError);
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
