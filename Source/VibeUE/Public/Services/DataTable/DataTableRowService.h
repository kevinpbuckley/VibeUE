// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/DataTable/Types/DataTableTypes.h"
#include "Core/Result.h"

// Forward declarations
class UDataTable;
class UScriptStruct;

/**
 * @class FDataTableRowService
 * @brief Service for CRUD operations on data table rows
 * 
 * This service provides functionality for:
 * - Adding, updating, removing rows
 * - Getting and setting row data
 * - Bulk operations (add multiple rows, clear all)
 * - JSON serialization/deserialization for rows
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @see FServiceBase
 * @see TResult
 */
class VIBEUE_API FDataTableRowService : public FServiceBase
{
public:
	/**
	 * @brief Constructor
	 * @param Context Service context for shared state (can be nullptr)
	 */
	explicit FDataTableRowService(TSharedPtr<FServiceContext> Context);

	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("DataTableRowService"); }

	// ========== Row Queries ==========

	/**
	 * @brief List all row names in a data table
	 * 
	 * @param DataTable The data table to query
	 * @return TResult containing array of row names or error
	 */
	TResult<TArray<FName>> ListRowNames(UDataTable* DataTable);

	/**
	 * @brief Get a row's data as JSON object
	 * 
	 * @param DataTable The data table containing the row
	 * @param RowName The row key to retrieve
	 * @return TResult containing JSON object of row data or error
	 */
	TResult<TSharedPtr<FJsonObject>> GetRow(UDataTable* DataTable, FName RowName);

	/**
	 * @brief Get all rows as a JSON object (row_name -> row_data)
	 * 
	 * @param DataTable The data table to export
	 * @param MaxRows Optional limit on rows returned (0 = all)
	 * @return TResult containing JSON object or error
	 */
	TResult<TSharedPtr<FJsonObject>> GetAllRows(UDataTable* DataTable, int32 MaxRows = 0);

	// ========== Row Mutations ==========

	/**
	 * @brief Add a new row to the data table
	 * 
	 * @param DataTable The data table to add to
	 * @param RowName The unique row key
	 * @param RowData JSON object with property values (optional, defaults to struct defaults)
	 * @return TResult containing FRowOperationResult or error
	 */
	TResult<FRowOperationResult> AddRow(
		UDataTable* DataTable,
		FName RowName,
		const TSharedPtr<FJsonObject>& RowData = nullptr);

	/**
	 * @brief Update an existing row's values
	 * 
	 * Supports partial updates - only specified properties are modified.
	 * 
	 * @param DataTable The data table containing the row
	 * @param RowName The row key to update
	 * @param RowData JSON object with property values to update
	 * @return TResult containing FRowOperationResult or error
	 */
	TResult<FRowOperationResult> UpdateRow(
		UDataTable* DataTable,
		FName RowName,
		const TSharedPtr<FJsonObject>& RowData);

	/**
	 * @brief Remove a row from the data table
	 * 
	 * @param DataTable The data table to remove from
	 * @param RowName The row key to remove
	 * @return TResult containing FRowOperationResult or error
	 */
	TResult<FRowOperationResult> RemoveRow(UDataTable* DataTable, FName RowName);

	/**
	 * @brief Rename a row's key
	 * 
	 * @param DataTable The data table containing the row
	 * @param OldName Current row key
	 * @param NewName New row key
	 * @return TResult containing FRowOperationResult or error
	 */
	TResult<FRowOperationResult> RenameRow(UDataTable* DataTable, FName OldName, FName NewName);

	// ========== Bulk Operations ==========

	/**
	 * @brief Add multiple rows at once
	 * 
	 * @param DataTable The data table to add to
	 * @param Rows Map of row_name -> row_data JSON objects
	 * @return TResult containing FBulkRowResult or error
	 */
	TResult<FBulkRowResult> AddRows(
		UDataTable* DataTable,
		const TMap<FName, TSharedPtr<FJsonObject>>& Rows);

	/**
	 * @brief Clear all rows from a data table
	 * 
	 * @param DataTable The data table to clear
	 * @return TResult containing number of rows cleared or error
	 */
	TResult<int32> ClearRows(UDataTable* DataTable);

	/**
	 * @brief Import rows from JSON, with merge or replace mode
	 * 
	 * @param DataTable The data table to import into
	 * @param JsonData JSON object with row_name -> row_data
	 * @param bReplace If true, clear existing rows first. If false, merge/update.
	 * @return TResult containing FBulkRowResult or error
	 */
	TResult<FBulkRowResult> ImportFromJson(
		UDataTable* DataTable,
		const TSharedPtr<FJsonObject>& JsonData,
		bool bReplace = false);

	// ========== Serialization Helpers ==========

	/**
	 * @brief Serialize a row to JSON object
	 * 
	 * @param RowStruct The struct type of the row
	 * @param RowData Pointer to the row data
	 * @return JSON object representation of the row
	 */
	TSharedPtr<FJsonObject> RowToJson(const UScriptStruct* RowStruct, void* RowData);

	/**
	 * @brief Deserialize JSON object to row data
	 * 
	 * @param RowStruct The struct type of the row
	 * @param RowData Pointer to the row data to populate
	 * @param JsonObj JSON object with row data
	 * @param OutError Error message if failed
	 * @return true if successful
	 */
	bool JsonToRow(
		const UScriptStruct* RowStruct,
		void* RowData,
		const TSharedPtr<FJsonObject>& JsonObj,
		FString& OutError);

	/**
	 * @brief Serialize a single property to JSON value
	 * 
	 * @param Property The property to serialize
	 * @param Container Pointer to the struct containing the property
	 * @return JSON value representation
	 */
	TSharedPtr<FJsonValue> PropertyToJson(FProperty* Property, void* Container);

	/**
	 * @brief Set a property from a JSON value
	 * 
	 * @param Property The property to set
	 * @param Container Pointer to the struct containing the property
	 * @param Value JSON value to set
	 * @param OutError Error message if failed
	 * @return true if successful
	 */
	bool JsonToProperty(
		FProperty* Property,
		void* Container,
		const TSharedPtr<FJsonValue>& Value,
		FString& OutError);
};
