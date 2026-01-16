// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UDataTableService.h"
#include "Engine/DataTable.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataTableFactory.h"
#include "UObject/UObjectIterator.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataTableService, Log, All);

// ============================================
// Helper Functions
// ============================================

UDataTable* UDataTableService::LoadDataTable(const FString& TablePath)
{
	if (TablePath.IsEmpty())
	{
		return nullptr;
	}

	UDataTable* DataTable = Cast<UDataTable>(UEditorAssetLibrary::LoadAsset(TablePath));
	if (!DataTable)
	{
		UE_LOG(LogDataTableService, Warning, TEXT("Failed to load DataTable: %s"), *TablePath);
	}
	return DataTable;
}

UScriptStruct* UDataTableService::FindRowStruct(const FString& StructNameOrPath)
{
	if (StructNameOrPath.IsEmpty())
	{
		return nullptr;
	}

	UScriptStruct* TableRowBase = FTableRowBase::StaticStruct();
	UScriptStruct* FoundStruct = nullptr;

	// Try direct find by path
	FoundStruct = FindObject<UScriptStruct>(nullptr, *StructNameOrPath);

	if (!FoundStruct)
	{
		// Try loading by path
		FoundStruct = LoadObject<UScriptStruct>(nullptr, *StructNameOrPath);
	}

	if (!FoundStruct)
	{
		// Search by name through all structs
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			UScriptStruct* Struct = *It;

			if (!Struct->IsChildOf(TableRowBase))
			{
				continue;
			}

			if (Struct->GetName().Equals(StructNameOrPath, ESearchCase::IgnoreCase))
			{
				FoundStruct = Struct;
				break;
			}

			// Also try without F prefix
			FString NameWithF = TEXT("F") + StructNameOrPath;
			if (Struct->GetName().Equals(NameWithF, ESearchCase::IgnoreCase))
			{
				FoundStruct = Struct;
				break;
			}
		}
	}

	// Verify it's a valid row struct
	if (FoundStruct && !FoundStruct->IsChildOf(TableRowBase))
	{
		UE_LOG(LogDataTableService, Warning, TEXT("%s is not a valid row struct"), *StructNameOrPath);
		return nullptr;
	}

	return FoundStruct;
}

bool UDataTableService::ShouldExposeProperty(FProperty* Property)
{
	if (!Property)
	{
		return false;
	}

	// Skip deprecated properties
	if (Property->HasAnyPropertyFlags(CPF_Deprecated))
	{
		return false;
	}

	// Skip transient properties
	if (Property->HasAnyPropertyFlags(CPF_Transient))
	{
		return false;
	}

	return true;
}

FString UDataTableService::GetPropertyTypeString(FProperty* Property)
{
	if (!Property)
	{
		return TEXT("Unknown");
	}

	if (CastField<FBoolProperty>(Property)) return TEXT("Bool");
	if (CastField<FIntProperty>(Property)) return TEXT("Int32");
	if (CastField<FInt64Property>(Property)) return TEXT("Int64");
	if (CastField<FFloatProperty>(Property)) return TEXT("Float");
	if (CastField<FDoubleProperty>(Property)) return TEXT("Double");
	if (CastField<FStrProperty>(Property)) return TEXT("String");
	if (CastField<FNameProperty>(Property)) return TEXT("Name");
	if (CastField<FTextProperty>(Property)) return TEXT("Text");

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		if (UEnum* Enum = EnumProp->GetEnum())
		{
			return FString::Printf(TEXT("Enum:%s"), *Enum->GetName());
		}
		return TEXT("Enum");
	}

	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (UEnum* Enum = ByteProp->Enum)
		{
			return FString::Printf(TEXT("Enum:%s"), *Enum->GetName());
		}
		return TEXT("Byte");
	}

	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		return FString::Printf(TEXT("Object:%s"), *ObjProp->PropertyClass->GetName());
	}

	if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(Property))
	{
		return FString::Printf(TEXT("SoftObject:%s"), *SoftProp->PropertyClass->GetName());
	}

	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		return FString::Printf(TEXT("Array<%s>"), *GetPropertyTypeString(ArrayProp->Inner));
	}

	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		return FString::Printf(TEXT("Struct:%s"), *StructProp->Struct->GetName());
	}

	if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		return FString::Printf(TEXT("Map<%s,%s>"),
			*GetPropertyTypeString(MapProp->KeyProp),
			*GetPropertyTypeString(MapProp->ValueProp));
	}

	return Property->GetCPPType();
}

// ============================================
// Discovery Actions
// ============================================

TArray<FRowStructTypeInfo> UDataTableService::SearchRowTypes(const FString& SearchFilter)
{
	TArray<FRowStructTypeInfo> Results;

	UScriptStruct* TableRowBase = FTableRowBase::StaticStruct();
	if (!TableRowBase)
	{
		return Results;
	}

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;

		// Must be a subclass of FTableRowBase
		if (!Struct->IsChildOf(TableRowBase))
		{
			continue;
		}

		// Skip FTableRowBase itself
		if (Struct == TableRowBase)
		{
			continue;
		}

		FString StructName = Struct->GetName();
		FString StructPath = Struct->GetPathName();

		// Apply search filter if provided
		if (!SearchFilter.IsEmpty())
		{
			if (!StructName.Contains(SearchFilter, ESearchCase::IgnoreCase) &&
				!StructPath.Contains(SearchFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		FRowStructTypeInfo Info;
		Info.Name = StructName;
		Info.Path = StructPath;

		// Get module info
		UPackage* Package = Struct->GetOutermost();
		if (Package)
		{
			Info.Module = Package->GetName();
		}

		// Check if native
		Info.bIsNative = !Struct->HasMetaData(TEXT("BlueprintType")) ||
						  Struct->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn);

		// Get parent struct
		if (UScriptStruct* SuperStruct = Cast<UScriptStruct>(Struct->GetSuperStruct()))
		{
			Info.ParentStruct = SuperStruct->GetName();
		}

		// Get property names
		for (TFieldIterator<FProperty> PropIt(Struct, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (ShouldExposeProperty(Property))
			{
				Info.PropertyNames.Add(Property->GetName());
			}
		}

		Results.Add(Info);
	}

	// Sort by name
	Results.Sort([](const FRowStructTypeInfo& A, const FRowStructTypeInfo& B) {
		return A.Name < B.Name;
	});

	return Results;
}

TArray<FDataTableInfo> UDataTableService::ListDataTables(const FString& RowStructFilter, const FString& PathFilter)
{
	TArray<FDataTableInfo> Results;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine.DataTable")));
	
	if (!PathFilter.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*PathFilter));
		Filter.bRecursivePaths = true;
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		FDataTableInfo Info;
		Info.Name = AssetData.AssetName.ToString();
		Info.Path = AssetData.GetObjectPathString();

		// Get row struct info from tags
		FAssetDataTagMapSharedView::FFindTagResult RowStructTag = AssetData.TagsAndValues.FindTag(TEXT("RowStructure"));
		if (RowStructTag.IsSet())
		{
			Info.RowStructPath = RowStructTag.GetValue();
			// Extract just the name from the path
			int32 LastDot;
			if (Info.RowStructPath.FindLastChar('.', LastDot))
			{
				Info.RowStruct = Info.RowStructPath.Mid(LastDot + 1);
			}
			else
			{
				Info.RowStruct = Info.RowStructPath;
			}
		}

		// Apply row struct filter
		if (!RowStructFilter.IsEmpty())
		{
			if (!Info.RowStruct.Contains(RowStructFilter, ESearchCase::IgnoreCase) &&
				!Info.RowStructPath.Contains(RowStructFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		// Get row count by loading the asset
		if (UDataTable* DataTable = Cast<UDataTable>(AssetData.GetAsset()))
		{
			Info.RowCount = DataTable->GetRowNames().Num();
		}

		Results.Add(Info);
	}

	// Sort by name
	Results.Sort([](const FDataTableInfo& A, const FDataTableInfo& B) {
		return A.Name < B.Name;
	});

	return Results;
}

// ============================================
// Lifecycle Actions
// ============================================

FString UDataTableService::CreateDataTable(const FString& RowStructName, const FString& AssetPath, const FString& AssetName)
{
	if (RowStructName.IsEmpty() || AssetName.IsEmpty())
	{
		UE_LOG(LogDataTableService, Warning, TEXT("CreateDataTable: RowStructName and AssetName are required"));
		return FString();
	}

	// Find the row struct
	UScriptStruct* RowStruct = FindRowStruct(RowStructName);
	if (!RowStruct)
	{
		UE_LOG(LogDataTableService, Warning, TEXT("CreateDataTable: Row struct not found: %s"), *RowStructName);
		return FString();
	}

	// Normalize asset path
	FString NormalizedPath = AssetPath;
	if (NormalizedPath.IsEmpty())
	{
		NormalizedPath = TEXT("/Game/Data");
	}

	if (!NormalizedPath.StartsWith(TEXT("/Game")) &&
		!NormalizedPath.StartsWith(TEXT("/Engine")))
	{
		if (NormalizedPath.StartsWith(TEXT("/")))
		{
			NormalizedPath = TEXT("/Game") + NormalizedPath;
		}
		else
		{
			NormalizedPath = TEXT("/Game/") + NormalizedPath;
		}
	}

	// Create the data table using asset tools
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Create factory with the specified row struct
	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = RowStruct;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, NormalizedPath, UDataTable::StaticClass(), Factory);

	if (!NewAsset)
	{
		UE_LOG(LogDataTableService, Warning, TEXT("CreateDataTable: Failed to create DataTable at %s/%s"), *NormalizedPath, *AssetName);
		return FString();
	}

	UDataTable* DataTable = Cast<UDataTable>(NewAsset);
	if (!DataTable)
	{
		UE_LOG(LogDataTableService, Warning, TEXT("CreateDataTable: Created asset is not a DataTable"));
		return FString();
	}

	// Mark dirty
	DataTable->MarkPackageDirty();

	UE_LOG(LogDataTableService, Display, TEXT("Created DataTable: %s with row struct: %s"),
		*DataTable->GetPathName(), *RowStruct->GetName());

	return DataTable->GetPathName();
}

// ============================================
// Info Actions
// ============================================

bool UDataTableService::GetInfo(const FString& TablePath, FDataTableDetailedInfo& OutInfo)
{
	UDataTable* DataTable = LoadDataTable(TablePath);
	if (!DataTable)
	{
		return false;
	}

	OutInfo.Name = DataTable->GetName();
	OutInfo.Path = DataTable->GetPathName();

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (RowStruct)
	{
		OutInfo.RowStruct = RowStruct->GetName();
		OutInfo.RowStructPath = RowStruct->GetPathName();
	}

	// Get row names
	TArray<FName> RowNames = DataTable->GetRowNames();
	OutInfo.RowCount = RowNames.Num();
	for (const FName& RowName : RowNames)
	{
		OutInfo.RowNames.Add(RowName.ToString());
	}

	// Build columns JSON
	TArray<TSharedPtr<FJsonValue>> ColumnsArray;
	if (RowStruct)
	{
		for (TFieldIterator<FProperty> PropIt(RowStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!ShouldExposeProperty(Property))
			{
				continue;
			}

			TSharedPtr<FJsonObject> ColObj = MakeShared<FJsonObject>();
			ColObj->SetStringField(TEXT("name"), Property->GetName());
			ColObj->SetStringField(TEXT("type"), GetPropertyTypeString(Property));
			ColObj->SetStringField(TEXT("cpp_type"), Property->GetCPPType());

			if (Property->HasMetaData(TEXT("Category")))
			{
				ColObj->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
			}
			if (Property->HasMetaData(TEXT("Tooltip")))
			{
				ColObj->SetStringField(TEXT("tooltip"), Property->GetMetaData(TEXT("Tooltip")));
			}

			bool bEditable = !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_BlueprintReadOnly);
			ColObj->SetBoolField(TEXT("editable"), bEditable);

			ColumnsArray.Add(MakeShared<FJsonValueObject>(ColObj));
		}
	}

	// Serialize columns to JSON string
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ColumnsArray, Writer);
	OutInfo.ColumnsJson = JsonString;

	return true;
}

TArray<FRowStructColumnInfo> UDataTableService::GetRowStruct(const FString& TablePathOrStructName)
{
	TArray<FRowStructColumnInfo> Results;

	const UScriptStruct* RowStruct = nullptr;

	// First try to load as DataTable
	UDataTable* DataTable = LoadDataTable(TablePathOrStructName);
	if (DataTable)
	{
		RowStruct = DataTable->GetRowStruct();
	}
	else
	{
		// Try as struct name
		RowStruct = FindRowStruct(TablePathOrStructName);
	}

	if (!RowStruct)
	{
		UE_LOG(LogDataTableService, Warning, TEXT("GetRowStruct: Could not find row struct for: %s"), *TablePathOrStructName);
		return Results;
	}

	for (TFieldIterator<FProperty> PropIt(RowStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!ShouldExposeProperty(Property))
		{
			continue;
		}

		FRowStructColumnInfo Column;
		Column.Name = Property->GetName();
		Column.Type = GetPropertyTypeString(Property);
		Column.CppType = Property->GetCPPType();

		if (Property->HasMetaData(TEXT("Category")))
		{
			Column.Category = Property->GetMetaData(TEXT("Category"));
		}
		if (Property->HasMetaData(TEXT("Tooltip")))
		{
			Column.Tooltip = Property->GetMetaData(TEXT("Tooltip"));
		}

		Column.bEditable = !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_BlueprintReadOnly);

		Results.Add(Column);
	}

	return Results;
}

// ============================================
// Row Operations
// ============================================

TArray<FString> UDataTableService::ListRows(const FString& TablePath)
{
	TArray<FString> Results;

	UDataTable* DataTable = LoadDataTable(TablePath);
	if (!DataTable)
	{
		return Results;
	}

	TArray<FName> RowNames = DataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		Results.Add(RowName.ToString());
	}

	return Results;
}

FString UDataTableService::GetRow(const FString& TablePath, const FString& RowName)
{
	UDataTable* DataTable = LoadDataTable(TablePath);
	if (!DataTable)
	{
		return FString();
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return FString();
	}

	void* RowData = DataTable->FindRowUnchecked(FName(*RowName));
	if (!RowData)
	{
		UE_LOG(LogDataTableService, Warning, TEXT("GetRow: Row '%s' not found in table"), *RowName);
		return FString();
	}

	TSharedPtr<FJsonObject> JsonObj = RowToJson(RowStruct, RowData);
	if (!JsonObj.IsValid())
	{
		return FString();
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

	return JsonString;
}

bool UDataTableService::AddRow(const FString& TablePath, const FString& RowName, const FString& DataJson)
{
	UDataTable* DataTable = LoadDataTable(TablePath);
	if (!DataTable)
	{
		return false;
	}

	if (RowName.IsEmpty())
	{
		UE_LOG(LogDataTableService, Warning, TEXT("AddRow: Row name is required"));
		return false;
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return false;
	}

	// Check if row already exists
	if (DataTable->FindRowUnchecked(FName(*RowName)))
	{
		UE_LOG(LogDataTableService, Warning, TEXT("AddRow: Row '%s' already exists. Use UpdateRow to modify it."), *RowName);
		return false;
	}

	// Allocate memory for new row and initialize with defaults
	void* NewRowData = FMemory::Malloc(RowStruct->GetStructureSize());
	RowStruct->InitializeStruct(NewRowData);

	// Apply provided data if any
	if (!DataJson.IsEmpty())
	{
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DataJson);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			FString Error;
			if (!JsonToRow(RowStruct, NewRowData, JsonObj, Error))
			{
				UE_LOG(LogDataTableService, Warning, TEXT("AddRow: Failed to apply row data: %s"), *Error);
			}
		}
	}

	// Add row to table
	DataTable->AddRow(FName(*RowName), *static_cast<FTableRowBase*>(NewRowData));

	// Cleanup temporary row data
	RowStruct->DestroyStruct(NewRowData);
	FMemory::Free(NewRowData);

	// Mark table dirty
	DataTable->MarkPackageDirty();

	return true;
}

bool UDataTableService::AddRows(const FString& TablePath, const FString& RowsJson, FBulkRowOperationResult& OutResult)
{
	UDataTable* DataTable = LoadDataTable(TablePath);
	if (!DataTable)
	{
		return false;
	}

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RowsJson);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogDataTableService, Warning, TEXT("AddRows: Invalid JSON format"));
		return false;
	}

	OutResult.TotalCount = JsonObj->Values.Num();

	for (const auto& Pair : JsonObj->Values)
	{
		const TSharedPtr<FJsonObject>* RowData;
		FString RowDataJson;

		if (Pair.Value->TryGetObject(RowData))
		{
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RowDataJson);
			FJsonSerializer::Serialize((*RowData).ToSharedRef(), Writer);
		}

		if (AddRow(TablePath, Pair.Key, RowDataJson))
		{
			OutResult.SucceededRows.Add(Pair.Key);
		}
		else
		{
			OutResult.FailedRows.Add(Pair.Key);
			OutResult.FailedReasons.Add(TEXT("Failed to add row"));
		}
	}

	return OutResult.FailedRows.Num() == 0;
}

bool UDataTableService::UpdateRow(const FString& TablePath, const FString& RowName, const FString& DataJson)
{
	UDataTable* DataTable = LoadDataTable(TablePath);
	if (!DataTable)
	{
		return false;
	}

	if (RowName.IsEmpty() || DataJson.IsEmpty())
	{
		UE_LOG(LogDataTableService, Warning, TEXT("UpdateRow: Row name and data are required"));
		return false;
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return false;
	}

	// Find existing row
	void* ExistingRowData = DataTable->FindRowUnchecked(FName(*RowName));
	if (!ExistingRowData)
	{
		UE_LOG(LogDataTableService, Warning, TEXT("UpdateRow: Row '%s' not found. Use AddRow to create it."), *RowName);
		return false;
	}

	// Parse JSON
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DataJson);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogDataTableService, Warning, TEXT("UpdateRow: Invalid JSON format"));
		return false;
	}

	// Apply updates (partial update)
	for (auto& Pair : JsonObj->Values)
	{
		FProperty* Property = RowStruct->FindPropertyByName(*Pair.Key);
		if (!Property)
		{
			UE_LOG(LogDataTableService, Warning, TEXT("UpdateRow: Property '%s' not found"), *Pair.Key);
			continue;
		}

		FString Error;
		if (!JsonToProperty(Property, ExistingRowData, Pair.Value, Error))
		{
			UE_LOG(LogDataTableService, Warning, TEXT("UpdateRow: Failed to set '%s': %s"), *Pair.Key, *Error);
		}
	}

	// Mark table dirty
	DataTable->MarkPackageDirty();

	return true;
}

bool UDataTableService::RemoveRow(const FString& TablePath, const FString& RowName)
{
	UDataTable* DataTable = LoadDataTable(TablePath);
	if (!DataTable)
	{
		return false;
	}

	if (RowName.IsEmpty())
	{
		return false;
	}

	// Check if row exists
	if (!DataTable->FindRowUnchecked(FName(*RowName)))
	{
		UE_LOG(LogDataTableService, Warning, TEXT("RemoveRow: Row '%s' not found"), *RowName);
		return false;
	}

	// Remove the row
	DataTable->RemoveRow(FName(*RowName));

	// Mark table dirty
	DataTable->MarkPackageDirty();

	return true;
}

bool UDataTableService::RenameRow(const FString& TablePath, const FString& OldName, const FString& NewName)
{
	UDataTable* DataTable = LoadDataTable(TablePath);
	if (!DataTable)
	{
		return false;
	}

	if (OldName.IsEmpty() || NewName.IsEmpty())
	{
		return false;
	}

	if (OldName == NewName)
	{
		return false;
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return false;
	}

	// Check if old row exists
	void* OldRowData = DataTable->FindRowUnchecked(FName(*OldName));
	if (!OldRowData)
	{
		UE_LOG(LogDataTableService, Warning, TEXT("RenameRow: Row '%s' not found"), *OldName);
		return false;
	}

	// Check if new name already exists
	if (DataTable->FindRowUnchecked(FName(*NewName)))
	{
		UE_LOG(LogDataTableService, Warning, TEXT("RenameRow: Row '%s' already exists"), *NewName);
		return false;
	}

	// Copy the row data
	void* CopiedRowData = FMemory::Malloc(RowStruct->GetStructureSize());
	RowStruct->InitializeStruct(CopiedRowData);
	RowStruct->CopyScriptStruct(CopiedRowData, OldRowData);

	// Add new row
	DataTable->AddRow(FName(*NewName), *static_cast<FTableRowBase*>(CopiedRowData));

	// Remove old row
	DataTable->RemoveRow(FName(*OldName));

	// Cleanup
	RowStruct->DestroyStruct(CopiedRowData);
	FMemory::Free(CopiedRowData);

	// Mark table dirty
	DataTable->MarkPackageDirty();

	return true;
}

int32 UDataTableService::ClearRows(const FString& TablePath)
{
	UDataTable* DataTable = LoadDataTable(TablePath);
	if (!DataTable)
	{
		return 0;
	}

	int32 RowCount = DataTable->GetRowNames().Num();

	// Empty the table
	DataTable->EmptyTable();

	// Mark table dirty
	DataTable->MarkPackageDirty();

	return RowCount;
}

// ============================================
// JSON Serialization Helpers
// ============================================

TSharedPtr<FJsonObject> UDataTableService::RowToJson(const UScriptStruct* RowStruct, void* RowData)
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

bool UDataTableService::JsonToRow(
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
			continue;
		}

		FString PropertyError;
		if (!JsonToProperty(Property, RowData, Pair.Value, PropertyError))
		{
			if (bAllSuccess)
			{
				OutError = FString::Printf(TEXT("Failed to set '%s': %s"), *Pair.Key, *PropertyError);
				bAllSuccess = false;
			}
		}
	}

	return bAllSuccess;
}

TSharedPtr<FJsonValue> UDataTableService::PropertyToJson(FProperty* Property, void* Container)
{
	if (!Property || !Container)
	{
		return MakeShared<FJsonValueNull>();
	}

	// Get the value pointer from the container
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	return ValuePtrToJson(Property, ValuePtr);
}

TSharedPtr<FJsonValue> UDataTableService::ValuePtrToJson(FProperty* Property, void* ValuePtr)
{
	if (!Property || !ValuePtr)
	{
		return MakeShared<FJsonValueNull>();
	}

	// Numeric types
	if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
	{
		if (NumericProp->IsFloatingPoint())
		{
			double Value = NumericProp->GetFloatingPointPropertyValue(ValuePtr);
			return MakeShared<FJsonValueNumber>(Value);
		}
		else
		{
			int64 Value = NumericProp->GetSignedIntPropertyValue(ValuePtr);
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

	// Array - use ValuePtrToJson for elements (they ARE the value, not a container)
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> JsonArray;
		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);

		for (int32 i = 0; i < ArrayHelper.Num(); ++i)
		{
			void* ElementPtr = ArrayHelper.GetRawPtr(i);
			TSharedPtr<FJsonValue> ElementValue = ValuePtrToJson(ArrayProp->Inner, ElementPtr);
			JsonArray.Add(ElementValue);
		}

		return MakeShared<FJsonValueArray>(JsonArray);
	}

	// Struct - ValuePtr IS the struct base, so inner properties use it as container
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		TSharedPtr<FJsonObject> StructObj = MakeShared<FJsonObject>();
		UScriptStruct* Struct = StructProp->Struct;

		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* InnerProp = *It;
			// ValuePtr is the struct base, so use PropertyToJson which computes offset
			TSharedPtr<FJsonValue> InnerValue = PropertyToJson(InnerProp, ValuePtr);
			StructObj->SetField(InnerProp->GetName(), InnerValue);
		}

		return MakeShared<FJsonValueObject>(StructObj);
	}

	// Map - use ValuePtrToJson for values (they ARE the value, not a container)
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

				TSharedPtr<FJsonValue> Value = ValuePtrToJson(MapProp->ValueProp, ValPtr);
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

bool UDataTableService::JsonToProperty(
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
	return JsonToValuePtr(Property, ValuePtr, Value, OutError);
}

bool UDataTableService::JsonToValuePtr(
	FProperty* Property,
	void* ValuePtr,
	const TSharedPtr<FJsonValue>& Value,
	FString& OutError)
{
	if (!Property || !ValuePtr || !Value.IsValid())
	{
		OutError = TEXT("Invalid parameters");
		return false;
	}

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

	// Array - use JsonToValuePtr for elements (they ARE the value, not a container)
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

			if (!JsonToValuePtr(ArrayProp->Inner, ElementPtr, (*JsonArray)[i], ElementError))
			{
				OutError = FString::Printf(TEXT("Array element %d: %s"), i, *ElementError);
				return false;
			}
		}
		return true;
	}

	// Struct - ValuePtr IS the struct base, so inner properties use JsonToProperty
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
					// ValuePtr is the struct base, use JsonToProperty which computes offset
					if (!JsonToProperty(InnerProp, ValuePtr, Pair.Value, InnerError))
					{
						UE_LOG(LogDataTableService, Warning, TEXT("Failed to set struct member %s: %s"), *Pair.Key, *InnerError);
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
