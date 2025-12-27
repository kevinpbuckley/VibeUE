// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declarations
class FServiceContext;
class UDataTable;

/**
 * Handler class for Data Table management MCP commands
 * 
 * Provides reflection-based discovery and manipulation of UDataTable assets.
 * Supports creating, loading, modifying rows, and managing data tables with full
 * row structure reflection for any row type.
 * 
 * Key differences from DataAsset:
 * - DataTables use row structs (FTableRowBase) instead of properties
 * - Each row has a unique FName key
 * - Row structure is defined by a UScriptStruct
 */
class VIBEUE_API FDataTableCommands
{
public:
	FDataTableCommands();

	/**
	 * Main command handler - routes to specific action handlers
	 * @param CommandType The command type (should be "manage_data_table")
	 * @param Params JSON parameters including "action" field
	 * @return JSON response with success/error status and data
	 */
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Service context for shared services
	TSharedPtr<FServiceContext> ServiceContext;

	// ========== Help ==========
	TSharedPtr<FJsonObject> HandleHelp(const TSharedPtr<FJsonObject>& Params);

	// ========== Discovery Actions ==========
	
	/** List all available row struct types that can be used for data tables */
	TSharedPtr<FJsonObject> HandleSearchRowTypes(const TSharedPtr<FJsonObject>& Params);
	
	/** List all data tables in the project */
	TSharedPtr<FJsonObject> HandleList(const TSharedPtr<FJsonObject>& Params);

	// ========== Table Lifecycle ==========
	
	/** Create a new data table with a specified row struct */
	TSharedPtr<FJsonObject> HandleCreate(const TSharedPtr<FJsonObject>& Params);

	// ========== Table Info ==========
	
	/** Get all information about a data table including row structure and rows */
	TSharedPtr<FJsonObject> HandleGetInfo(const TSharedPtr<FJsonObject>& Params);
	
	/** Get the row structure definition */
	TSharedPtr<FJsonObject> HandleGetRowStruct(const TSharedPtr<FJsonObject>& Params);

	// ========== Row Operations ==========
	
	/** List all rows in a data table */
	TSharedPtr<FJsonObject> HandleListRows(const TSharedPtr<FJsonObject>& Params);
	
	/** Get a specific row by name */
	TSharedPtr<FJsonObject> HandleGetRow(const TSharedPtr<FJsonObject>& Params);
	
	/** Add a new row to the data table */
	TSharedPtr<FJsonObject> HandleAddRow(const TSharedPtr<FJsonObject>& Params);
	
	/** Update an existing row */
	TSharedPtr<FJsonObject> HandleUpdateRow(const TSharedPtr<FJsonObject>& Params);
	
	/** Remove a row from the data table */
	TSharedPtr<FJsonObject> HandleRemoveRow(const TSharedPtr<FJsonObject>& Params);
	
	/** Rename a row key */
	TSharedPtr<FJsonObject> HandleRenameRow(const TSharedPtr<FJsonObject>& Params);

	// ========== Bulk Operations ==========
	
	/** Add multiple rows at once */
	TSharedPtr<FJsonObject> HandleAddRows(const TSharedPtr<FJsonObject>& Params);
	
	/** Clear all rows from the data table */
	TSharedPtr<FJsonObject> HandleClearRows(const TSharedPtr<FJsonObject>& Params);

	// ========== Import/Export ==========
	
	/** Import rows from JSON */
	TSharedPtr<FJsonObject> HandleImportJson(const TSharedPtr<FJsonObject>& Params);
	
	/** Export rows to JSON */
	TSharedPtr<FJsonObject> HandleExportJson(const TSharedPtr<FJsonObject>& Params);

	// ========== Helper Functions ==========
	
	/**
	 * Load a data table by path or name
	 * @param TablePath Full path like "/Game/Data/MyTable" or just "MyTable"
	 * @return Pointer to the loaded data table, or nullptr if not found
	 */
	UDataTable* LoadDataTable(const FString& TablePath);
	
	/**
	 * Find a row struct by name or path
	 * @param StructNameOrPath Struct name like "MyRowStruct" or path like "/Script/MyGame.MyRowStruct"
	 * @return UScriptStruct pointer or nullptr
	 */
	UScriptStruct* FindRowStruct(const FString& StructNameOrPath);
	
	/**
	 * Serialize a row to JSON
	 * @param RowStruct The struct type of the row
	 * @param RowData Pointer to the row data
	 * @return JSON object representation of the row
	 */
	TSharedPtr<FJsonObject> RowToJson(const UScriptStruct* RowStruct, void* RowData);
	
	/**
	 * Deserialize a row from JSON
	 * @param RowStruct The struct type of the row
	 * @param RowData Pointer to the row data to populate
	 * @param JsonObj JSON object with row data
	 * @param OutError Error message if failed
	 * @return true if successful
	 */
	bool JsonToRow(const UScriptStruct* RowStruct, void* RowData, const TSharedPtr<FJsonObject>& JsonObj, FString& OutError);
	
	/**
	 * Serialize a property to JSON value
	 * @param Property The property to serialize
	 * @param Container Pointer to the struct containing the property
	 * @return JSON value representation
	 */
	TSharedPtr<FJsonValue> PropertyToJson(FProperty* Property, void* Container);
	
	/**
	 * Set a property from a JSON value
	 * @param Property The property to set
	 * @param Container Pointer to the struct containing the property
	 * @param Value JSON value to set
	 * @param OutError Error message if failed
	 * @return true if successful
	 */
	bool JsonToProperty(FProperty* Property, void* Container, const TSharedPtr<FJsonValue>& Value, FString& OutError);
	
	/**
	 * Get property type information as a descriptive string
	 * @param Property The property
	 * @return Type string like "float", "FVector", "TArray<FString>", etc.
	 */
	FString GetPropertyTypeString(FProperty* Property);
	
	/**
	 * Check if a property should be exposed to the tool
	 * @param Property The property to check
	 * @return true if the property should be visible
	 */
	bool ShouldExposeProperty(FProperty* Property);
	
	/**
	 * Get row struct column definitions
	 * @param RowStruct The row struct to analyze
	 * @return Array of column definitions
	 */
	TArray<TSharedPtr<FJsonValue>> GetColumnDefinitions(UScriptStruct* RowStruct);

	// Response helpers
	TSharedPtr<FJsonObject> CreateSuccessResponse(const FString& Message = TEXT(""));
	TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage, const FString& ErrorCode = TEXT("ERROR"));
	TSharedPtr<FJsonObject> CreateErrorResponseWithParams(const FString& ErrorMessage, const TArray<FString>& ValidParams);
};
