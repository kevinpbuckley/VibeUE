// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Commands/DataTableCommands.h"
#include "Services/DataTable/DataTableDiscoveryService.h"
#include "Services/DataTable/DataTableRowService.h"
#include "Services/DataTable/DataTableLifecycleService.h"
#include "Core/ServiceContext.h"
#include "Utils/HelpFileReader.h"
#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataTableCommands, Log, All);

FDataTableCommands::FDataTableCommands()
{
	ServiceContext = MakeShared<FServiceContext>();
}

TSharedPtr<FJsonObject> FDataTableCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType != TEXT("manage_data_table"))
	{
		return CreateErrorResponse(TEXT("Unknown command type"), TEXT("INVALID_COMMAND"));
	}
	
	if (!Params.IsValid())
	{
		return CreateErrorResponse(TEXT("Parameters are required"));
	}
	
	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action))
	{
		return CreateErrorResponse(TEXT("action parameter is required"));
	}
	
	Action = Action.ToLower();
	UE_LOG(LogDataTableCommands, Display, TEXT("DataTableCommands: Handling action '%s'"), *Action);
	
	// Route to action handlers
	if (Action == TEXT("help"))
	{
		return HandleHelp(Params);
	}
	else if (Action == TEXT("search_row_types") || Action == TEXT("list_row_types") || Action == TEXT("get_available_row_types"))
	{
		return HandleSearchRowTypes(Params);
	}
	else if (Action == TEXT("list"))
	{
		return HandleList(Params);
	}
	else if (Action == TEXT("create"))
	{
		return HandleCreate(Params);
	}
	else if (Action == TEXT("get_info"))
	{
		return HandleGetInfo(Params);
	}
	else if (Action == TEXT("get_row_struct"))
	{
		return HandleGetRowStruct(Params);
	}
	else if (Action == TEXT("list_rows"))
	{
		return HandleListRows(Params);
	}
	else if (Action == TEXT("get_row"))
	{
		return HandleGetRow(Params);
	}
	else if (Action == TEXT("add_row"))
	{
		return HandleAddRow(Params);
	}
	else if (Action == TEXT("update_row"))
	{
		return HandleUpdateRow(Params);
	}
	else if (Action == TEXT("remove_row") || Action == TEXT("delete_row"))
	{
		return HandleRemoveRow(Params);
	}
	else if (Action == TEXT("rename_row"))
	{
		return HandleRenameRow(Params);
	}
	else if (Action == TEXT("add_rows"))
	{
		return HandleAddRows(Params);
	}
	else if (Action == TEXT("clear_rows"))
	{
		return HandleClearRows(Params);
	}
	else if (Action == TEXT("import_json"))
	{
		return HandleImportJson(Params);
	}
	else if (Action == TEXT("export_json"))
	{
		return HandleExportJson(Params);
	}
	else
	{
		return CreateErrorResponse(FString::Printf(TEXT("Unknown action: %s. Use action='help' for available actions."), *Action));
	}
}

// ========== Help ==========

TSharedPtr<FJsonObject> FDataTableCommands::HandleHelp(const TSharedPtr<FJsonObject>& Params)
{
	return FHelpFileReader::HandleHelp(TEXT("manage_data_table"), Params);
}

// ========== Discovery Actions ==========

TSharedPtr<FJsonObject> FDataTableCommands::HandleSearchRowTypes(const TSharedPtr<FJsonObject>& Params)
{
	FString SearchFilter;
	Params->TryGetStringField(TEXT("search_filter"), SearchFilter);
	Params->TryGetStringField(TEXT("search_text"), SearchFilter); // Alias
	
	FDataTableDiscoveryService Service(ServiceContext);
	auto Result = Service.SearchRowStructTypes(SearchFilter);
	
	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorMessage(), Result.GetErrorCode());
	}
	
	TArray<TSharedPtr<FJsonValue>> TypesArray;
	for (const FRowStructInfo& Info : Result.GetValue())
	{
		TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
		TypeObj->SetStringField(TEXT("name"), Info.Name);
		TypeObj->SetStringField(TEXT("path"), Info.Path);
		TypeObj->SetStringField(TEXT("module"), Info.Module);
		TypeObj->SetStringField(TEXT("parent_struct"), Info.ParentStruct);
		TypeObj->SetBoolField(TEXT("is_native"), Info.bIsNative);
		
		TArray<TSharedPtr<FJsonValue>> PropsArray;
		for (const FString& PropName : Info.PropertyNames)
		{
			PropsArray.Add(MakeShared<FJsonValueString>(PropName));
		}
		TypeObj->SetArrayField(TEXT("properties"), PropsArray);
		
		TypesArray.Add(MakeShared<FJsonValueObject>(TypeObj));
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetArrayField(TEXT("types"), TypesArray);
	Response->SetNumberField(TEXT("count"), TypesArray.Num());
	
	if (!SearchFilter.IsEmpty())
	{
		Response->SetStringField(TEXT("filter"), SearchFilter);
	}
	
	return Response;
}

TSharedPtr<FJsonObject> FDataTableCommands::HandleList(const TSharedPtr<FJsonObject>& Params)
{
	FString RowStructFilter;
	FString PathFilter = TEXT("/Game");
	
	Params->TryGetStringField(TEXT("row_struct"), RowStructFilter);
	Params->TryGetStringField(TEXT("path"), PathFilter);
	
	FDataTableDiscoveryService Service(ServiceContext);
	auto Result = Service.ListDataTables(RowStructFilter, PathFilter);
	
	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorMessage(), Result.GetErrorCode());
	}
	
	TArray<TSharedPtr<FJsonValue>> TablesArray;
	for (const FDataTableInfo& Info : Result.GetValue())
	{
		TSharedPtr<FJsonObject> TableObj = MakeShared<FJsonObject>();
		TableObj->SetStringField(TEXT("name"), Info.Name);
		TableObj->SetStringField(TEXT("path"), Info.Path);
		TableObj->SetStringField(TEXT("row_struct"), Info.RowStructName);
		TableObj->SetStringField(TEXT("row_struct_path"), Info.RowStructPath);
		
		TablesArray.Add(MakeShared<FJsonValueObject>(TableObj));
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetArrayField(TEXT("tables"), TablesArray);
	Response->SetNumberField(TEXT("count"), TablesArray.Num());
	
	return Response;
}

// ========== Table Lifecycle ==========

TSharedPtr<FJsonObject> FDataTableCommands::HandleCreate(const TSharedPtr<FJsonObject>& Params)
{
	FString RowStructName;
	FString AssetPath;
	FString AssetName;
	
	if (!Params->TryGetStringField(TEXT("row_struct"), RowStructName))
	{
		Params->TryGetStringField(TEXT("struct_name"), RowStructName);
	}
	
	if (RowStructName.IsEmpty())
	{
		return CreateErrorResponse(TEXT("row_struct is required. Use search_row_types to find available row structs."));
	}
	
	Params->TryGetStringField(TEXT("asset_path"), AssetPath);
	Params->TryGetStringField(TEXT("asset_name"), AssetName);
	
	// If full path with name provided
	if (!AssetPath.IsEmpty() && AssetName.IsEmpty())
	{
		int32 LastSlash;
		if (AssetPath.FindLastChar('/', LastSlash))
		{
			FString PotentialName = AssetPath.Mid(LastSlash + 1);
			if (!PotentialName.IsEmpty() && !PotentialName.Contains(TEXT(".")))
			{
				AssetName = PotentialName;
				AssetPath = AssetPath.Left(LastSlash);
			}
		}
	}
	
	if (AssetName.IsEmpty())
	{
		return CreateErrorResponse(TEXT("asset_name is required (or include it in asset_path)"));
	}
	
	FDataTableLifecycleService Service(ServiceContext);
	auto Result = Service.CreateDataTableByStructName(RowStructName, AssetPath, AssetName);
	
	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorMessage(), Result.GetErrorCode());
	}
	
	UDataTable* DataTable = Result.GetValue();
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse(
		FString::Printf(TEXT("Created data table: %s"), *DataTable->GetPathName()));
	Response->SetStringField(TEXT("asset_path"), DataTable->GetPathName());
	Response->SetStringField(TEXT("asset_name"), DataTable->GetName());
	Response->SetStringField(TEXT("row_struct"), DataTable->GetRowStruct()->GetName());
	
	return Response;
}

// ========== Table Info ==========

TSharedPtr<FJsonObject> FDataTableCommands::HandleGetInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
	{
		return CreateErrorResponse(TEXT("table_path is required"));
	}
	
	bool bIncludeRows = true;
	int32 MaxRows = 0;
	Params->TryGetBoolField(TEXT("include_rows"), bIncludeRows);
	Params->TryGetNumberField(TEXT("max_rows"), MaxRows);
	
	FDataTableDiscoveryService DiscoveryService(ServiceContext);
	auto TableResult = DiscoveryService.FindDataTable(TablePath);
	
	if (TableResult.IsError())
	{
		return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
	}
	
	UDataTable* DataTable = TableResult.GetValue();
	
	auto InfoResult = DiscoveryService.GetDataTableInfo(DataTable, true);
	if (InfoResult.IsError())
	{
		return CreateErrorResponse(InfoResult.GetErrorMessage(), InfoResult.GetErrorCode());
	}
	
	const FDataTableInfo& Info = InfoResult.GetValue();
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("name"), Info.Name);
	Response->SetStringField(TEXT("path"), Info.Path);
	Response->SetStringField(TEXT("row_struct"), Info.RowStructName);
	Response->SetStringField(TEXT("row_struct_path"), Info.RowStructPath);
	Response->SetNumberField(TEXT("row_count"), Info.RowCount);
	
	// Add columns
	TArray<TSharedPtr<FJsonValue>> ColumnsArray;
	for (const FColumnInfo& Col : Info.Columns)
	{
		TSharedPtr<FJsonObject> ColObj = MakeShared<FJsonObject>();
		ColObj->SetStringField(TEXT("name"), Col.Name);
		ColObj->SetStringField(TEXT("type"), Col.Type);
		ColObj->SetStringField(TEXT("cpp_type"), Col.CppType);
		if (!Col.Category.IsEmpty())
		{
			ColObj->SetStringField(TEXT("category"), Col.Category);
		}
		if (!Col.Tooltip.IsEmpty())
		{
			ColObj->SetStringField(TEXT("tooltip"), Col.Tooltip);
		}
		ColObj->SetBoolField(TEXT("editable"), Col.bEditable);
		ColumnsArray.Add(MakeShared<FJsonValueObject>(ColObj));
	}
	Response->SetArrayField(TEXT("columns"), ColumnsArray);
	
	// Add rows if requested
	if (bIncludeRows)
	{
		FDataTableRowService RowService(ServiceContext);
		auto RowsResult = RowService.GetAllRows(DataTable, MaxRows);
		if (RowsResult.IsSuccess())
		{
			Response->SetObjectField(TEXT("rows"), RowsResult.GetValue());
		}
	}
	
	return Response;
}

TSharedPtr<FJsonObject> FDataTableCommands::HandleGetRowStruct(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	FString StructName;
	
	Params->TryGetStringField(TEXT("table_path"), TablePath);
	Params->TryGetStringField(TEXT("struct_name"), StructName);
	
	FDataTableDiscoveryService Service(ServiceContext);
	
	const UScriptStruct* RowStruct = nullptr;
	
	if (!TablePath.IsEmpty())
	{
		auto TableResult = Service.FindDataTable(TablePath);
		if (TableResult.IsError())
		{
			return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
		}
		RowStruct = TableResult.GetValue()->GetRowStruct();
	}
	else if (!StructName.IsEmpty())
	{
		auto StructResult = Service.FindRowStruct(StructName);
		if (StructResult.IsError())
		{
			return CreateErrorResponse(StructResult.GetErrorMessage(), StructResult.GetErrorCode());
		}
		RowStruct = StructResult.GetValue();
	}
	else
	{
		return CreateErrorResponse(TEXT("Either table_path or struct_name is required"));
	}
	
	auto ColumnsResult = Service.GetRowStructColumns(RowStruct);
	if (ColumnsResult.IsError())
	{
		return CreateErrorResponse(ColumnsResult.GetErrorMessage(), ColumnsResult.GetErrorCode());
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("struct_name"), RowStruct->GetName());
	Response->SetStringField(TEXT("struct_path"), RowStruct->GetPathName());
	
	TArray<TSharedPtr<FJsonValue>> ColumnsArray;
	for (const FColumnInfo& Col : ColumnsResult.GetValue())
	{
		TSharedPtr<FJsonObject> ColObj = MakeShared<FJsonObject>();
		ColObj->SetStringField(TEXT("name"), Col.Name);
		ColObj->SetStringField(TEXT("type"), Col.Type);
		ColObj->SetStringField(TEXT("cpp_type"), Col.CppType);
		if (!Col.Category.IsEmpty())
		{
			ColObj->SetStringField(TEXT("category"), Col.Category);
		}
		if (!Col.Tooltip.IsEmpty())
		{
			ColObj->SetStringField(TEXT("tooltip"), Col.Tooltip);
		}
		ColObj->SetBoolField(TEXT("editable"), Col.bEditable);
		ColumnsArray.Add(MakeShared<FJsonValueObject>(ColObj));
	}
	Response->SetArrayField(TEXT("columns"), ColumnsArray);
	
	return Response;
}

// ========== Row Operations ==========

TSharedPtr<FJsonObject> FDataTableCommands::HandleListRows(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
	{
		return CreateErrorResponse(TEXT("table_path is required"));
	}
	
	FDataTableDiscoveryService DiscoveryService(ServiceContext);
	auto TableResult = DiscoveryService.FindDataTable(TablePath);
	
	if (TableResult.IsError())
	{
		return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
	}
	
	FDataTableRowService RowService(ServiceContext);
	auto RowsResult = RowService.ListRowNames(TableResult.GetValue());
	
	if (RowsResult.IsError())
	{
		return CreateErrorResponse(RowsResult.GetErrorMessage(), RowsResult.GetErrorCode());
	}
	
	TArray<TSharedPtr<FJsonValue>> RowsArray;
	for (const FName& RowName : RowsResult.GetValue())
	{
		RowsArray.Add(MakeShared<FJsonValueString>(RowName.ToString()));
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("table_path"), TablePath);
	Response->SetArrayField(TEXT("rows"), RowsArray);
	Response->SetNumberField(TEXT("count"), RowsArray.Num());
	
	return Response;
}

TSharedPtr<FJsonObject> FDataTableCommands::HandleGetRow(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	FString RowName;
	
	if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
	{
		return CreateErrorResponse(TEXT("table_path is required"));
	}
	
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
	{
		return CreateErrorResponse(TEXT("row_name is required"));
	}
	
	FDataTableDiscoveryService DiscoveryService(ServiceContext);
	auto TableResult = DiscoveryService.FindDataTable(TablePath);
	
	if (TableResult.IsError())
	{
		return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
	}
	
	FDataTableRowService RowService(ServiceContext);
	auto RowResult = RowService.GetRow(TableResult.GetValue(), FName(*RowName));
	
	if (RowResult.IsError())
	{
		return CreateErrorResponse(RowResult.GetErrorMessage(), RowResult.GetErrorCode());
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("table_path"), TablePath);
	Response->SetStringField(TEXT("row_name"), RowName);
	Response->SetObjectField(TEXT("data"), RowResult.GetValue());
	
	return Response;
}

TSharedPtr<FJsonObject> FDataTableCommands::HandleAddRow(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	FString RowName;
	
	if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
	{
		return CreateErrorResponse(TEXT("table_path is required"));
	}
	
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
	{
		return CreateErrorResponse(TEXT("row_name is required"));
	}
	
	const TSharedPtr<FJsonObject>* DataObj = nullptr;
	Params->TryGetObjectField(TEXT("data"), DataObj);
	
	FDataTableDiscoveryService DiscoveryService(ServiceContext);
	auto TableResult = DiscoveryService.FindDataTable(TablePath);
	
	if (TableResult.IsError())
	{
		return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
	}
	
	FDataTableRowService RowService(ServiceContext);
	auto AddResult = RowService.AddRow(
		TableResult.GetValue(),
		FName(*RowName),
		DataObj ? *DataObj : nullptr);
	
	if (AddResult.IsError())
	{
		return CreateErrorResponse(AddResult.GetErrorMessage(), AddResult.GetErrorCode());
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse(
		FString::Printf(TEXT("Added row '%s' to %s"), *RowName, *TablePath));
	Response->SetStringField(TEXT("table_path"), TablePath);
	Response->SetStringField(TEXT("row_name"), RowName);
	
	return Response;
}

TSharedPtr<FJsonObject> FDataTableCommands::HandleUpdateRow(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	FString RowName;
	
	if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
	{
		return CreateErrorResponse(TEXT("table_path is required"));
	}
	
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
	{
		return CreateErrorResponse(TEXT("row_name is required"));
	}
	
	const TSharedPtr<FJsonObject>* DataObj = nullptr;
	if (!Params->TryGetObjectField(TEXT("data"), DataObj) || !DataObj)
	{
		return CreateErrorResponse(TEXT("data is required for update_row"));
	}
	
	FDataTableDiscoveryService DiscoveryService(ServiceContext);
	auto TableResult = DiscoveryService.FindDataTable(TablePath);
	
	if (TableResult.IsError())
	{
		return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
	}
	
	FDataTableRowService RowService(ServiceContext);
	auto UpdateResult = RowService.UpdateRow(TableResult.GetValue(), FName(*RowName), *DataObj);
	
	if (UpdateResult.IsError())
	{
		return CreateErrorResponse(UpdateResult.GetErrorMessage(), UpdateResult.GetErrorCode());
	}
	
	const FRowOperationResult& OpResult = UpdateResult.GetValue();
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse(
		FString::Printf(TEXT("Updated row '%s' in %s"), *RowName, *TablePath));
	Response->SetStringField(TEXT("table_path"), TablePath);
	Response->SetStringField(TEXT("row_name"), RowName);
	
	TArray<TSharedPtr<FJsonValue>> ModifiedArray;
	for (const FString& Prop : OpResult.ModifiedProperties)
	{
		ModifiedArray.Add(MakeShared<FJsonValueString>(Prop));
	}
	Response->SetArrayField(TEXT("updated_properties"), ModifiedArray);
	
	return Response;
}

TSharedPtr<FJsonObject> FDataTableCommands::HandleRemoveRow(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	FString RowName;
	
	if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
	{
		return CreateErrorResponse(TEXT("table_path is required"));
	}
	
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
	{
		return CreateErrorResponse(TEXT("row_name is required"));
	}
	
	FDataTableDiscoveryService DiscoveryService(ServiceContext);
	auto TableResult = DiscoveryService.FindDataTable(TablePath);
	
	if (TableResult.IsError())
	{
		return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
	}
	
	FDataTableRowService RowService(ServiceContext);
	auto RemoveResult = RowService.RemoveRow(TableResult.GetValue(), FName(*RowName));
	
	if (RemoveResult.IsError())
	{
		return CreateErrorResponse(RemoveResult.GetErrorMessage(), RemoveResult.GetErrorCode());
	}
	
	return CreateSuccessResponse(FString::Printf(TEXT("Removed row '%s' from %s"), *RowName, *TablePath));
}

TSharedPtr<FJsonObject> FDataTableCommands::HandleRenameRow(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	FString OldName;
	FString NewName;
	
	if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
	{
		return CreateErrorResponse(TEXT("table_path is required"));
	}
	
	if (!Params->TryGetStringField(TEXT("row_name"), OldName))
	{
		return CreateErrorResponse(TEXT("row_name is required"));
	}
	
	if (!Params->TryGetStringField(TEXT("new_name"), NewName))
	{
		return CreateErrorResponse(TEXT("new_name is required"));
	}
	
	FDataTableDiscoveryService DiscoveryService(ServiceContext);
	auto TableResult = DiscoveryService.FindDataTable(TablePath);
	
	if (TableResult.IsError())
	{
		return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
	}
	
	FDataTableRowService RowService(ServiceContext);
	auto RenameResult = RowService.RenameRow(TableResult.GetValue(), FName(*OldName), FName(*NewName));
	
	if (RenameResult.IsError())
	{
		return CreateErrorResponse(RenameResult.GetErrorMessage(), RenameResult.GetErrorCode());
	}
	
	return CreateSuccessResponse(FString::Printf(TEXT("Renamed row '%s' to '%s' in %s"), *OldName, *NewName, *TablePath));
}

// ========== Bulk Operations ==========

TSharedPtr<FJsonObject> FDataTableCommands::HandleAddRows(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
	{
		return CreateErrorResponse(TEXT("table_path is required"));
	}
	
	const TSharedPtr<FJsonObject>* RowsObj = nullptr;
	if (!Params->TryGetObjectField(TEXT("rows"), RowsObj) || !RowsObj)
	{
		return CreateErrorResponse(TEXT("rows is required (object with row_name keys and data values)"));
	}
	
	FDataTableDiscoveryService DiscoveryService(ServiceContext);
	auto TableResult = DiscoveryService.FindDataTable(TablePath);
	
	if (TableResult.IsError())
	{
		return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
	}
	
	// Convert JSON to map
	TMap<FName, TSharedPtr<FJsonObject>> RowsMap;
	for (const auto& Pair : (*RowsObj)->Values)
	{
		const TSharedPtr<FJsonObject>* RowData;
		if (Pair.Value->TryGetObject(RowData))
		{
			RowsMap.Add(FName(*Pair.Key), *RowData);
		}
	}
	
	FDataTableRowService RowService(ServiceContext);
	auto BulkResult = RowService.AddRows(TableResult.GetValue(), RowsMap);
	
	if (BulkResult.IsError())
	{
		return CreateErrorResponse(BulkResult.GetErrorMessage(), BulkResult.GetErrorCode());
	}
	
	const FBulkRowResult& Result = BulkResult.GetValue();
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse(
		FString::Printf(TEXT("Added %d rows to %s"), Result.SucceededRows.Num(), *TablePath));
	
	TArray<TSharedPtr<FJsonValue>> AddedArray;
	for (const FString& Row : Result.SucceededRows)
	{
		AddedArray.Add(MakeShared<FJsonValueString>(Row));
	}
	Response->SetArrayField(TEXT("added_rows"), AddedArray);
	
	if (Result.FailedRows.Num() > 0)
	{
		TSharedPtr<FJsonObject> FailedObj = MakeShared<FJsonObject>();
		for (const auto& Pair : Result.FailedRows)
		{
			FailedObj->SetStringField(Pair.Key, Pair.Value);
		}
		Response->SetObjectField(TEXT("failed_rows"), FailedObj);
	}
	
	return Response;
}

TSharedPtr<FJsonObject> FDataTableCommands::HandleClearRows(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
	{
		return CreateErrorResponse(TEXT("table_path is required"));
	}
	
	bool bConfirm = false;
	Params->TryGetBoolField(TEXT("confirm"), bConfirm);
	
	if (!bConfirm)
	{
		return CreateErrorResponse(TEXT("confirm=true is required for clear_rows (destructive operation)"));
	}
	
	FDataTableDiscoveryService DiscoveryService(ServiceContext);
	auto TableResult = DiscoveryService.FindDataTable(TablePath);
	
	if (TableResult.IsError())
	{
		return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
	}
	
	FDataTableRowService RowService(ServiceContext);
	auto ClearResult = RowService.ClearRows(TableResult.GetValue());
	
	if (ClearResult.IsError())
	{
		return CreateErrorResponse(ClearResult.GetErrorMessage(), ClearResult.GetErrorCode());
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse(
		FString::Printf(TEXT("Cleared %d rows from %s"), ClearResult.GetValue(), *TablePath));
	Response->SetNumberField(TEXT("cleared_count"), ClearResult.GetValue());
	
	return Response;
}

// ========== Import/Export ==========

TSharedPtr<FJsonObject> FDataTableCommands::HandleImportJson(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
	{
		return CreateErrorResponse(TEXT("table_path is required"));
	}
	
	const TSharedPtr<FJsonObject>* JsonData = nullptr;
	if (!Params->TryGetObjectField(TEXT("json_data"), JsonData) || !JsonData)
	{
		return CreateErrorResponse(TEXT("json_data is required"));
	}
	
	FString Mode;
	Params->TryGetStringField(TEXT("mode"), Mode);
	bool bReplace = Mode.Equals(TEXT("replace"), ESearchCase::IgnoreCase);
	
	FDataTableDiscoveryService DiscoveryService(ServiceContext);
	auto TableResult = DiscoveryService.FindDataTable(TablePath);
	
	if (TableResult.IsError())
	{
		return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
	}
	
	FDataTableRowService RowService(ServiceContext);
	auto ImportResult = RowService.ImportFromJson(TableResult.GetValue(), *JsonData, bReplace);
	
	if (ImportResult.IsError())
	{
		return CreateErrorResponse(ImportResult.GetErrorMessage(), ImportResult.GetErrorCode());
	}
	
	const FBulkRowResult& Result = ImportResult.GetValue();
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse(
		FString::Printf(TEXT("Imported %d rows to %s"), Result.SucceededRows.Num(), *TablePath));
	Response->SetNumberField(TEXT("imported_count"), Result.SucceededRows.Num());
	Response->SetStringField(TEXT("mode"), bReplace ? TEXT("replace") : TEXT("merge"));
	
	if (Result.FailedRows.Num() > 0)
	{
		Response->SetNumberField(TEXT("failed_count"), Result.FailedRows.Num());
	}
	
	return Response;
}

TSharedPtr<FJsonObject> FDataTableCommands::HandleExportJson(const TSharedPtr<FJsonObject>& Params)
{
	FString TablePath;
	if (!Params->TryGetStringField(TEXT("table_path"), TablePath))
	{
		return CreateErrorResponse(TEXT("table_path is required"));
	}
	
	FString Format;
	Params->TryGetStringField(TEXT("format"), Format);
	bool bArrayFormat = Format.Equals(TEXT("array"), ESearchCase::IgnoreCase);
	
	FDataTableDiscoveryService DiscoveryService(ServiceContext);
	auto TableResult = DiscoveryService.FindDataTable(TablePath);
	
	if (TableResult.IsError())
	{
		return CreateErrorResponse(TableResult.GetErrorMessage(), TableResult.GetErrorCode());
	}
	
	UDataTable* DataTable = TableResult.GetValue();
	
	FDataTableRowService RowService(ServiceContext);
	auto RowsResult = RowService.GetAllRows(DataTable, 0);
	
	if (RowsResult.IsError())
	{
		return CreateErrorResponse(RowsResult.GetErrorMessage(), RowsResult.GetErrorCode());
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("table_path"), TablePath);
	Response->SetStringField(TEXT("row_struct"), DataTable->GetRowStruct()->GetName());
	
	if (bArrayFormat)
	{
		// Convert to array format with _row_name field
		TArray<TSharedPtr<FJsonValue>> DataArray;
		for (const auto& Pair : RowsResult.GetValue()->Values)
		{
			const TSharedPtr<FJsonObject>* RowObj;
			if (Pair.Value->TryGetObject(RowObj))
			{
				TSharedPtr<FJsonObject> RowCopy = MakeShared<FJsonObject>();
				RowCopy->SetStringField(TEXT("_row_name"), Pair.Key);
				for (const auto& RowPair : (*RowObj)->Values)
				{
					RowCopy->SetField(RowPair.Key, RowPair.Value);
				}
				DataArray.Add(MakeShared<FJsonValueObject>(RowCopy));
			}
		}
		Response->SetArrayField(TEXT("data"), DataArray);
	}
	else
	{
		Response->SetObjectField(TEXT("data"), RowsResult.GetValue());
	}
	
	return Response;
}

// ========== Response Helpers ==========

TSharedPtr<FJsonObject> FDataTableCommands::CreateSuccessResponse(const FString& Message)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	if (!Message.IsEmpty())
	{
		Response->SetStringField(TEXT("message"), Message);
	}
	return Response;
}

TSharedPtr<FJsonObject> FDataTableCommands::CreateErrorResponse(const FString& ErrorMessage, const FString& ErrorCode)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error"), ErrorMessage);
	Response->SetStringField(TEXT("error_code"), ErrorCode);
	return Response;
}

TSharedPtr<FJsonObject> FDataTableCommands::CreateErrorResponseWithParams(const FString& ErrorMessage, const TArray<FString>& ValidParams)
{
	TSharedPtr<FJsonObject> Response = CreateErrorResponse(ErrorMessage, TEXT("MISSING_PARAMS"));
	
	TArray<TSharedPtr<FJsonValue>> ParamsArray;
	for (const FString& Param : ValidParams)
	{
		ParamsArray.Add(MakeShared<FJsonValueString>(Param));
	}
	Response->SetArrayField(TEXT("valid_params"), ParamsArray);
	
	return Response;
}

// ========== Helper Functions (delegated to services) ==========

UDataTable* FDataTableCommands::LoadDataTable(const FString& TablePath)
{
	FDataTableDiscoveryService Service(ServiceContext);
	auto Result = Service.LoadDataTable(TablePath);
	return Result.IsSuccess() ? Result.GetValue() : nullptr;
}

UScriptStruct* FDataTableCommands::FindRowStruct(const FString& StructNameOrPath)
{
	FDataTableDiscoveryService Service(ServiceContext);
	auto Result = Service.FindRowStruct(StructNameOrPath);
	return Result.IsSuccess() ? Result.GetValue() : nullptr;
}

TSharedPtr<FJsonObject> FDataTableCommands::RowToJson(const UScriptStruct* RowStruct, void* RowData)
{
	FDataTableRowService Service(ServiceContext);
	return Service.RowToJson(RowStruct, RowData);
}

bool FDataTableCommands::JsonToRow(const UScriptStruct* RowStruct, void* RowData, const TSharedPtr<FJsonObject>& JsonObj, FString& OutError)
{
	FDataTableRowService Service(ServiceContext);
	return Service.JsonToRow(RowStruct, RowData, JsonObj, OutError);
}

TSharedPtr<FJsonValue> FDataTableCommands::PropertyToJson(FProperty* Property, void* Container)
{
	FDataTableRowService Service(ServiceContext);
	return Service.PropertyToJson(Property, Container);
}

bool FDataTableCommands::JsonToProperty(FProperty* Property, void* Container, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	FDataTableRowService Service(ServiceContext);
	return Service.JsonToProperty(Property, Container, Value, OutError);
}

FString FDataTableCommands::GetPropertyTypeString(FProperty* Property)
{
	FDataTableDiscoveryService Service(ServiceContext);
	return Service.GetPropertyTypeString(Property);
}

bool FDataTableCommands::ShouldExposeProperty(FProperty* Property)
{
	FDataTableDiscoveryService Service(ServiceContext);
	return Service.ShouldExposeProperty(Property);
}

TArray<TSharedPtr<FJsonValue>> FDataTableCommands::GetColumnDefinitions(UScriptStruct* RowStruct)
{
	FDataTableDiscoveryService Service(ServiceContext);
	auto Result = Service.GetRowStructColumns(RowStruct);
	
	TArray<TSharedPtr<FJsonValue>> Columns;
	if (Result.IsSuccess())
	{
		for (const FColumnInfo& Col : Result.GetValue())
		{
			TSharedPtr<FJsonObject> ColObj = MakeShared<FJsonObject>();
			ColObj->SetStringField(TEXT("name"), Col.Name);
			ColObj->SetStringField(TEXT("type"), Col.Type);
			ColObj->SetStringField(TEXT("cpp_type"), Col.CppType);
			Columns.Add(MakeShared<FJsonValueObject>(ColObj));
		}
	}
	return Columns;
}
