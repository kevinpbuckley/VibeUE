// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Services/DataTable/DataTableDiscoveryService.h"
#include "Core/ErrorCodes.h"
#include "Engine/DataTable.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataTableDiscovery, Log, All);

FDataTableDiscoveryService::FDataTableDiscoveryService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

// ========== Row Struct Discovery ==========

TResult<TArray<FRowStructInfo>> FDataTableDiscoveryService::SearchRowStructTypes(const FString& SearchFilter)
{
	TArray<FRowStructInfo> Results;
	
	// Get the base FTableRowBase struct
	UScriptStruct* TableRowBase = FTableRowBase::StaticStruct();
	if (!TableRowBase)
	{
		return TResult<TArray<FRowStructInfo>>::Error(
			VibeUE::ErrorCodes::INTERNAL_ERROR,
			TEXT("Could not find FTableRowBase struct"));
	}
	
	// Iterate all script structs
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
		
		FRowStructInfo Info;
		Info.Name = StructName;
		Info.Path = StructPath;
		
		// Get module info
		UPackage* Package = Struct->GetOutermost();
		if (Package)
		{
			Info.Module = Package->GetName();
		}
		
		// Check if native or Blueprint
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
	Results.Sort([](const FRowStructInfo& A, const FRowStructInfo& B) {
		return A.Name < B.Name;
	});
	
	return TResult<TArray<FRowStructInfo>>::Success(MoveTemp(Results));
}

TResult<UScriptStruct*> FDataTableDiscoveryService::FindRowStruct(const FString& StructNameOrPath)
{
	if (StructNameOrPath.IsEmpty())
	{
		return TResult<UScriptStruct*>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Struct name or path is required"));
	}
	
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
		UScriptStruct* TableRowBase = FTableRowBase::StaticStruct();
		
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
	
	if (!FoundStruct)
	{
		return TResult<UScriptStruct*>::Error(
			VibeUE::ErrorCodes::DATATABLE_NOT_FOUND,
			FString::Printf(TEXT("Row struct not found: %s. Use search_row_types to find available structs."), *StructNameOrPath));
	}
	
	// Verify it's a valid row struct
	UScriptStruct* TableRowBase = FTableRowBase::StaticStruct();
	if (!FoundStruct->IsChildOf(TableRowBase))
	{
		return TResult<UScriptStruct*>::Error(
			VibeUE::ErrorCodes::ROW_STRUCT_INVALID,
			FString::Printf(TEXT("%s is not a valid row struct (must inherit from FTableRowBase)"), *StructNameOrPath));
	}
	
	return TResult<UScriptStruct*>::Success(FoundStruct);
}

TResult<TArray<FColumnInfo>> FDataTableDiscoveryService::GetRowStructColumns(const UScriptStruct* RowStruct)
{
	if (!RowStruct)
	{
		return TResult<TArray<FColumnInfo>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Row struct is required"));
	}
	
	TArray<FColumnInfo> Columns;
	
	for (TFieldIterator<FProperty> PropIt(RowStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		
		if (!ShouldExposeProperty(Property))
		{
			continue;
		}
		
		FColumnInfo Column;
		Column.Name = Property->GetName();
		Column.Type = GetPropertyTypeString(Property);
		Column.CppType = Property->GetCPPType();
		
		// Get metadata
		if (Property->HasMetaData(TEXT("Category")))
		{
			Column.Category = Property->GetMetaData(TEXT("Category"));
		}
		
		if (Property->HasMetaData(TEXT("ToolTip")))
		{
			Column.Tooltip = Property->GetMetaData(TEXT("ToolTip"));
		}
		
		Column.bEditable = Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible);
		
		Columns.Add(Column);
	}
	
	return TResult<TArray<FColumnInfo>>::Success(MoveTemp(Columns));
}

TResult<FRowStructInfo> FDataTableDiscoveryService::GetRowStructInfo(const FString& StructNameOrPath)
{
	auto StructResult = FindRowStruct(StructNameOrPath);
	if (StructResult.IsError())
	{
		return TResult<FRowStructInfo>::Error(StructResult.GetErrorCode(), StructResult.GetErrorMessage());
	}
	
	UScriptStruct* Struct = StructResult.GetValue();
	
	FRowStructInfo Info;
	Info.Name = Struct->GetName();
	Info.Path = Struct->GetPathName();
	
	UPackage* Package = Struct->GetOutermost();
	if (Package)
	{
		Info.Module = Package->GetName();
	}
	
	Info.bIsNative = !Struct->HasMetaData(TEXT("BlueprintType")) || 
					  Struct->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn);
	
	if (UScriptStruct* SuperStruct = Cast<UScriptStruct>(Struct->GetSuperStruct()))
	{
		Info.ParentStruct = SuperStruct->GetName();
	}
	
	// Get property names
	for (TFieldIterator<FProperty> PropIt(Struct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (ShouldExposeProperty(Property))
		{
			Info.PropertyNames.Add(Property->GetName());
		}
	}
	
	return TResult<FRowStructInfo>::Success(MoveTemp(Info));
}

// ========== Data Table Discovery ==========

TResult<TArray<FDataTableInfo>> FDataTableDiscoveryService::ListDataTables(
	const FString& RowStructFilter,
	const FString& PathFilter)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	
	TArray<FAssetData> AssetDataList;
	FTopLevelAssetPath DataTablePath(UDataTable::StaticClass()->GetClassPathName());
	AssetRegistry.GetAssetsByClass(DataTablePath, AssetDataList, true);
	
	TArray<FDataTableInfo> Results;
	
	for (const FAssetData& AssetData : AssetDataList)
	{
		FString AssetPath = AssetData.GetObjectPathString();
		
		// Apply path filter
		if (!PathFilter.IsEmpty() && !AssetPath.StartsWith(PathFilter))
		{
			continue;
		}
		
		FDataTableInfo Info;
		Info.Name = AssetData.AssetName.ToString();
		Info.Path = AssetPath;
		
		// Get row struct from asset metadata
		FString RowStructStr;
		if (AssetData.GetTagValue(FName(TEXT("RowStructure")), RowStructStr))
		{
			Info.RowStructPath = RowStructStr;
			// Extract just the name
			int32 LastDot;
			if (Info.RowStructPath.FindLastChar('.', LastDot))
			{
				Info.RowStructName = Info.RowStructPath.Mid(LastDot + 1);
			}
			else
			{
				Info.RowStructName = Info.RowStructPath;
			}
		}
		
		// Apply row struct filter
		if (!RowStructFilter.IsEmpty())
		{
			if (!Info.RowStructName.Contains(RowStructFilter, ESearchCase::IgnoreCase) &&
				!Info.RowStructPath.Contains(RowStructFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}
		
		// We'd need to load the asset to get row count, which can be expensive
		// Just leave it as 0 for the list view
		
		Results.Add(Info);
	}
	
	// Sort by name
	Results.Sort([](const FDataTableInfo& A, const FDataTableInfo& B) {
		return A.Name < B.Name;
	});
	
	return TResult<TArray<FDataTableInfo>>::Success(MoveTemp(Results));
}

TResult<UDataTable*> FDataTableDiscoveryService::FindDataTable(const FString& TableNameOrPath)
{
	if (TableNameOrPath.IsEmpty())
	{
		return TResult<UDataTable*>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Table name or path is required"));
	}
	
	// If it looks like a full path, try loading directly
	if (TableNameOrPath.StartsWith(TEXT("/")))
	{
		return LoadDataTable(TableNameOrPath);
	}
	
	// Search for the table by name
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	
	TArray<FAssetData> AssetDataList;
	FTopLevelAssetPath DataTablePath(UDataTable::StaticClass()->GetClassPathName());
	AssetRegistry.GetAssetsByClass(DataTablePath, AssetDataList, true);
	
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetName.ToString().Equals(TableNameOrPath, ESearchCase::IgnoreCase))
		{
			return LoadDataTable(AssetData.GetObjectPathString());
		}
	}
	
	return TResult<UDataTable*>::Error(
		VibeUE::ErrorCodes::DATATABLE_NOT_FOUND,
		FString::Printf(TEXT("Data table not found: %s. Use list action to find existing tables."), *TableNameOrPath));
}

TResult<UDataTable*> FDataTableDiscoveryService::LoadDataTable(const FString& TablePath)
{
	FString NormalizedPath = TablePath;
	
	// Add /Game prefix if needed
	if (!NormalizedPath.StartsWith(TEXT("/Game")) && 
		!NormalizedPath.StartsWith(TEXT("/Engine")) &&
		!NormalizedPath.StartsWith(TEXT("/Script")))
	{
		if (!NormalizedPath.StartsWith(TEXT("/")))
		{
			NormalizedPath = TEXT("/Game/") + NormalizedPath;
		}
		else
		{
			NormalizedPath = TEXT("/Game") + NormalizedPath;
		}
	}
	
	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *NormalizedPath);
	
	if (!DataTable)
	{
		// Try without .uasset extension
		NormalizedPath = NormalizedPath.Replace(TEXT(".uasset"), TEXT(""));
		DataTable = LoadObject<UDataTable>(nullptr, *NormalizedPath);
	}
	
	if (!DataTable)
	{
		return TResult<UDataTable*>::Error(
			VibeUE::ErrorCodes::DATATABLE_NOT_FOUND,
			FString::Printf(TEXT("Data table not found: %s"), *TablePath));
	}
	
	return TResult<UDataTable*>::Success(DataTable);
}

TResult<FDataTableInfo> FDataTableDiscoveryService::GetDataTableInfo(UDataTable* DataTable, bool bIncludeColumns)
{
	if (!DataTable)
	{
		return TResult<FDataTableInfo>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Data table is required"));
	}
	
	FDataTableInfo Info;
	Info.Name = DataTable->GetName();
	Info.Path = DataTable->GetPathName();
	
	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (RowStruct)
	{
		Info.RowStructName = RowStruct->GetName();
		Info.RowStructPath = RowStruct->GetPathName();
	}
	
	Info.RowCount = DataTable->GetRowNames().Num();
	
	if (bIncludeColumns && RowStruct)
	{
		auto ColumnsResult = GetRowStructColumns(RowStruct);
		if (ColumnsResult.IsSuccess())
		{
			Info.Columns = ColumnsResult.GetValue();
		}
	}
	
	return TResult<FDataTableInfo>::Success(MoveTemp(Info));
}

// ========== Property Reflection Helpers ==========

FString FDataTableDiscoveryService::GetPropertyTypeString(FProperty* Property) const
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
			if (CastField<FInt16Property>(Property)) return TEXT("int16");
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
	
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		return FString::Printf(TEXT("TArray<%s>"), *GetPropertyTypeString(ArrayProp->Inner));
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

bool FDataTableDiscoveryService::ShouldExposeProperty(FProperty* Property) const
{
	if (!Property)
	{
		return false;
	}
	
	// Skip deprecated
	if (Property->HasMetaData(TEXT("DeprecatedProperty")))
	{
		return false;
	}
	
	// For row structs, expose most properties since they're meant to be edited
	// Skip only truly internal properties
	if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
	{
		return false;
	}
	
	return true;
}
