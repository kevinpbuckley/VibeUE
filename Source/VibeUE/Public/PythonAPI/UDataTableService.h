// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UDataTableService.generated.h"

/**
 * Information about a DataTable
 */
USTRUCT(BlueprintType)
struct FDataTableDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString TableName;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString TablePath;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString RowStructType;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TArray<FString> RowNames;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	int32 RowCount = 0;
};

/**
 * DataTable service exposed directly to Python.
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all data tables
 *   tables = unreal.DataTableService.list_data_tables()
 *
 *   # Get table info
 *   info = unreal.DataTableDetailedInfo()
 *   if unreal.DataTableService.get_table_info("/Game/Data/DT_Items", info):
 *       print(f"Rows: {info.row_count}")
 *
 * @note This replaces the JSON-based manage_datatable MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UDataTableService : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * List all DataTable assets.
	 *
	 * @param RowStructFilter - Optional filter by row struct type
	 * @return Array of DataTable paths
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static TArray<FString> ListDataTables(const FString& RowStructFilter = TEXT(""));

	/**
	 * Get DataTable information including row structure and row names.
	 *
	 * @param TablePath - Full path to the DataTable
	 * @param OutInfo - Structure containing table details
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static bool GetTableInfo(const FString& TablePath, FDataTableDetailedInfo& OutInfo);

	/**
	 * Get all row names from a DataTable.
	 *
	 * @param TablePath - Full path to the DataTable
	 * @return Array of row names
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static TArray<FString> GetRowNames(const FString& TablePath);

	/**
	 * Get a specific row from a DataTable as a JSON string.
	 *
	 * @param TablePath - Full path to the DataTable
	 * @param RowName - Name of the row
	 * @return JSON string representation of the row data
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static FString GetRowAsJson(const FString& TablePath, const FString& RowName);
};
