// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "PythonAPI/UDataTableService.h"
#include "Engine/DataTable.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

TArray<FString> UDataTableService::ListDataTables(const FString& RowStructFilter)
{
	TArray<FString> DataTablePaths;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine.DataTable")));

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		// If filter specified, check row struct
		if (!RowStructFilter.IsEmpty())
		{
			FAssetDataTagMapSharedView::FFindTagResult RowStructTag = AssetData.TagsAndValues.FindTag(TEXT("RowStructure"));
			if (RowStructTag.IsSet())
			{
				FString RowStructPath = RowStructTag.GetValue();
				if (!RowStructPath.Contains(RowStructFilter))
				{
					continue;
				}
			}
		}

		DataTablePaths.Add(AssetData.GetObjectPathString());
	}

	return DataTablePaths;
}

bool UDataTableService::GetTableInfo(const FString& TablePath, FDataTableDetailedInfo& OutInfo)
{
	UDataTable* DataTable = Cast<UDataTable>(UEditorAssetLibrary::LoadAsset(TablePath));
	if (!DataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("UDataTableService::GetTableInfo: Failed to load DataTable: %s"), *TablePath);
		return false;
	}

	OutInfo.TableName = DataTable->GetName();
	OutInfo.TablePath = TablePath;

	if (const UScriptStruct* RowStruct = DataTable->GetRowStruct())
	{
		OutInfo.RowStructType = RowStruct->GetName();
	}

	// GetRowNames returns TArray<FName>, convert to TArray<FString>
	TArray<FName> RowNamesFName = DataTable->GetRowNames();
	for (const FName& RowName : RowNamesFName)
	{
		OutInfo.RowNames.Add(RowName.ToString());
	}
	OutInfo.RowNames.Sort([](const FString& A, const FString& B) { return A < B; });

	OutInfo.RowCount = OutInfo.RowNames.Num();

	return true;
}

TArray<FString> UDataTableService::GetRowNames(const FString& TablePath)
{
	UDataTable* DataTable = Cast<UDataTable>(UEditorAssetLibrary::LoadAsset(TablePath));
	if (!DataTable)
	{
		return TArray<FString>();
	}

	// GetRowNames returns TArray<FName>, convert to TArray<FString>
	TArray<FString> RowNames;
	TArray<FName> RowNamesFName = DataTable->GetRowNames();
	for (const FName& RowName : RowNamesFName)
	{
		RowNames.Add(RowName.ToString());
	}
	RowNames.Sort([](const FString& A, const FString& B) { return A < B; });

	return RowNames;
}

FString UDataTableService::GetRowAsJson(const FString& TablePath, const FString& RowName)
{
	UDataTable* DataTable = Cast<UDataTable>(UEditorAssetLibrary::LoadAsset(TablePath));
	if (!DataTable)
	{
		return FString();
	}

	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		return FString();
	}

	uint8* RowData = DataTable->FindRowUnchecked(FName(*RowName));
	if (!RowData)
	{
		return FString();
	}

	// Export row to JSON
	FString JsonString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	JsonWriter->WriteObjectStart();

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FProperty* Property = *It;
		if (Property)
		{
			FString PropertyName = Property->GetName();
			FString PropertyValue;
			Property->ExportTextItem_Direct(PropertyValue, Property->ContainerPtrToValuePtr<void>(RowData), nullptr, nullptr, PPF_None);

			JsonWriter->WriteValue(PropertyName, PropertyValue);
		}
	}

	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	return JsonString;
}
