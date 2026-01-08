// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UDataTableService.generated.h"

/**
 * Information about a row struct type.
 */
USTRUCT(BlueprintType)
struct FRowStructTypeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString Path;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString Module;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString ParentStruct;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	bool bIsNative = false;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TArray<FString> PropertyNames;
};

/**
 * Information about a DataTable asset.
 */
USTRUCT(BlueprintType)
struct FDataTableInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString Path;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString RowStruct;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString RowStructPath;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	int32 RowCount = 0;
};

/**
 * Detailed information about a DataTable including columns.
 */
USTRUCT(BlueprintType)
struct FDataTableDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString Path;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString RowStruct;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString RowStructPath;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	int32 RowCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TArray<FString> RowNames;

	/** JSON array of column definitions: [{name, type, cpp_type, category, editable}...] */
	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString ColumnsJson;
};

/**
 * Information about a row struct column/property.
 */
USTRUCT(BlueprintType)
struct FRowStructColumnInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString Type;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString CppType;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	FString Tooltip;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	bool bEditable = true;
};

/**
 * Result of a bulk row operation.
 */
USTRUCT(BlueprintType)
struct FBulkRowOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TArray<FString> SucceededRows;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TArray<FString> FailedRows;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TArray<FString> FailedReasons;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	int32 TotalCount = 0;
};

/**
 * DataTable service exposed directly to Python.
 *
 * Python Usage:
 *   import unreal
 *
 *   # Search for row struct types
 *   types = unreal.DataTableService.search_row_types("Item")
 *
 *   # List all data tables
 *   tables = unreal.DataTableService.list_data_tables("", "/Game")
 *
 *   # Create a new data table
 *   path = unreal.DataTableService.create_data_table("FMyRowStruct", "/Game/Data", "DT_MyTable")
 *
 *   # Get table info
 *   info = unreal.DataTableDetailedInfo()
 *   if unreal.DataTableService.get_info("/Game/Data/DT_Items", info):
 *       print(f"Rows: {info.row_count}")
 *
 *   # Row operations
 *   rows = unreal.DataTableService.list_rows("/Game/Data/DT_Items")
 *   row_json = unreal.DataTableService.get_row("/Game/Data/DT_Items", "Row1")
 *   unreal.DataTableService.add_row("/Game/Data/DT_Items", "NewRow", '{"Name":"Test"}')
 *
 * @note This replaces the JSON-based manage_data_table MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UDataTableService : public UObject
{
	GENERATED_BODY()

public:
	// ============================================
	// Discovery Actions
	// ============================================

	/**
	 * Search for row struct types that can be used in DataTables.
	 * Maps to action="search_row_types"
	 *
	 * @param SearchFilter - Optional filter to match struct names (case-insensitive)
	 * @return Array of row struct type info
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static TArray<FRowStructTypeInfo> SearchRowTypes(const FString& SearchFilter = TEXT(""));

	/**
	 * List all DataTable assets.
	 * Maps to action="list"
	 *
	 * @param RowStructFilter - Optional filter by row struct name
	 * @param PathFilter - Path to search in (default: /Game)
	 * @return Array of DataTable info
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static TArray<FDataTableInfo> ListDataTables(const FString& RowStructFilter = TEXT(""), const FString& PathFilter = TEXT("/Game"));

	// ============================================
	// Lifecycle Actions
	// ============================================

	/**
	 * Create a new DataTable asset.
	 * Maps to action="create"
	 *
	 * @param RowStructName - Name of the row struct type (e.g., "FMyRowStruct")
	 * @param AssetPath - Directory path for the new asset
	 * @param AssetName - Name for the new DataTable
	 * @return Full asset path if successful, empty string on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static FString CreateDataTable(const FString& RowStructName, const FString& AssetPath, const FString& AssetName);

	// ============================================
	// Info Actions
	// ============================================

	/**
	 * Get detailed information about a DataTable.
	 * Maps to action="get_info"
	 *
	 * @param TablePath - Full path to the DataTable
	 * @param OutInfo - Output struct with table details and columns
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static bool GetInfo(const FString& TablePath, FDataTableDetailedInfo& OutInfo);

	/**
	 * Get the row struct schema (columns) for a DataTable or struct name.
	 * Maps to action="get_row_struct"
	 *
	 * @param TablePathOrStructName - Either a table path or row struct name
	 * @return Array of column definitions
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static TArray<FRowStructColumnInfo> GetRowStruct(const FString& TablePathOrStructName);

	// ============================================
	// Row Operations
	// ============================================

	/**
	 * List all row names in a DataTable.
	 * Maps to action="list_rows"
	 *
	 * @param TablePath - Full path to the DataTable
	 * @return Array of row names
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static TArray<FString> ListRows(const FString& TablePath);

	/**
	 * Get a single row from a DataTable as JSON.
	 * Maps to action="get_row"
	 *
	 * @param TablePath - Full path to the DataTable
	 * @param RowName - Name of the row to retrieve
	 * @return JSON string with row data, empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static FString GetRow(const FString& TablePath, const FString& RowName);

	/**
	 * Add a new row to a DataTable.
	 * Maps to action="add_row"
	 *
	 * @param TablePath - Full path to the DataTable
	 * @param RowName - Name for the new row
	 * @param DataJson - JSON object with row property values
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static bool AddRow(const FString& TablePath, const FString& RowName, const FString& DataJson = TEXT(""));

	/**
	 * Add multiple rows to a DataTable at once.
	 * Maps to action="add_rows"
	 *
	 * @param TablePath - Full path to the DataTable
	 * @param RowsJson - JSON object where keys are row names and values are row data objects
	 * @param OutResult - Result details showing which rows succeeded/failed
	 * @return True if all rows were added successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static bool AddRows(const FString& TablePath, const FString& RowsJson, FBulkRowOperationResult& OutResult);

	/**
	 * Update an existing row in a DataTable.
	 * Maps to action="update_row"
	 *
	 * @param TablePath - Full path to the DataTable
	 * @param RowName - Name of the row to update
	 * @param DataJson - JSON object with properties to update (partial update supported)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static bool UpdateRow(const FString& TablePath, const FString& RowName, const FString& DataJson);

	/**
	 * Remove a row from a DataTable.
	 * Maps to action="remove_row"
	 *
	 * @param TablePath - Full path to the DataTable
	 * @param RowName - Name of the row to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static bool RemoveRow(const FString& TablePath, const FString& RowName);

	/**
	 * Rename a row in a DataTable.
	 * Maps to action="rename_row"
	 *
	 * @param TablePath - Full path to the DataTable
	 * @param OldName - Current name of the row
	 * @param NewName - New name for the row
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static bool RenameRow(const FString& TablePath, const FString& OldName, const FString& NewName);

	/**
	 * Remove all rows from a DataTable.
	 * Maps to action="clear_rows"
	 *
	 * @param TablePath - Full path to the DataTable
	 * @return Number of rows that were removed
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataTables")
	static int32 ClearRows(const FString& TablePath);

private:
	// Helper to load a DataTable by path
	static UDataTable* LoadDataTable(const FString& TablePath);

	// Helper to find a row struct by name
	static UScriptStruct* FindRowStruct(const FString& StructNameOrPath);

	// Helper to check if a property should be exposed
	static bool ShouldExposeProperty(FProperty* Property);

	// Helper to get property type string
	static FString GetPropertyTypeString(FProperty* Property);

	// Helper to serialize row to JSON
	static TSharedPtr<FJsonObject> RowToJson(const UScriptStruct* RowStruct, void* RowData);

	// Helper to deserialize JSON to row
	static bool JsonToRow(const UScriptStruct* RowStruct, void* RowData, const TSharedPtr<FJsonObject>& JsonObj, FString& OutError);

	// Helper property serialization
	static TSharedPtr<FJsonValue> PropertyToJson(FProperty* Property, void* Container);
	static bool JsonToProperty(FProperty* Property, void* Container, const TSharedPtr<FJsonValue>& Value, FString& OutError);
};
