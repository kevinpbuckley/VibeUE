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
 * @class FDataTableDiscoveryService
 * @brief Service for discovering data tables and row struct types
 * 
 * This service provides functionality for:
 * - Finding and listing data tables in the project
 * - Discovering available row struct types (FTableRowBase subclasses)
 * - Loading data tables by path or name
 * - Getting schema information about row structs
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @see FServiceBase
 * @see TResult
 */
class VIBEUE_API FDataTableDiscoveryService : public FServiceBase
{
public:
	/**
	 * @brief Constructor
	 * @param Context Service context for shared state (can be nullptr)
	 */
	explicit FDataTableDiscoveryService(TSharedPtr<FServiceContext> Context);

	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("DataTableDiscoveryService"); }

	// ========== Row Struct Discovery ==========

	/**
	 * @brief Search for available row struct types
	 * 
	 * Finds all UScriptStruct types that inherit from FTableRowBase and can be
	 * used as row types for data tables.
	 * 
	 * @param SearchFilter Optional filter to match against struct names (case-insensitive)
	 * @return TResult containing array of FRowStructInfo or error
	 */
	TResult<TArray<FRowStructInfo>> SearchRowStructTypes(const FString& SearchFilter = TEXT(""));

	/**
	 * @brief Find a row struct by name or path
	 * 
	 * @param StructNameOrPath Struct name (e.g., "FMyItemRow") or path (e.g., "/Script/MyGame.FMyItemRow")
	 * @return TResult containing UScriptStruct pointer or error
	 */
	TResult<UScriptStruct*> FindRowStruct(const FString& StructNameOrPath);

	/**
	 * @brief Get detailed schema information for a row struct
	 * 
	 * Returns column definitions including property types, categories, tooltips.
	 * 
	 * @param RowStruct The row struct to analyze
	 * @return TResult containing array of FColumnInfo or error
	 */
	TResult<TArray<FColumnInfo>> GetRowStructColumns(const UScriptStruct* RowStruct);

	/**
	 * @brief Get detailed schema information by struct name
	 * 
	 * @param StructNameOrPath Struct name or path
	 * @return TResult containing FRowStructInfo with columns or error
	 */
	TResult<FRowStructInfo> GetRowStructInfo(const FString& StructNameOrPath);

	// ========== Data Table Discovery ==========

	/**
	 * @brief List all data tables in the project
	 * 
	 * @param RowStructFilter Optional filter by row struct type
	 * @param PathFilter Optional filter by asset path prefix (default: /Game)
	 * @return TResult containing array of FDataTableInfo or error
	 */
	TResult<TArray<FDataTableInfo>> ListDataTables(
		const FString& RowStructFilter = TEXT(""),
		const FString& PathFilter = TEXT("/Game"));

	/**
	 * @brief Find a data table by name or path
	 * 
	 * @param TableNameOrPath Table name or full path
	 * @return TResult containing UDataTable pointer or error
	 */
	TResult<UDataTable*> FindDataTable(const FString& TableNameOrPath);

	/**
	 * @brief Load a data table from a specific path
	 * 
	 * @param TablePath Full asset path to the data table
	 * @return TResult containing UDataTable pointer or error
	 */
	TResult<UDataTable*> LoadDataTable(const FString& TablePath);

	/**
	 * @brief Get detailed information about a data table
	 * 
	 * @param DataTable The data table to analyze
	 * @param bIncludeColumns Whether to include column definitions
	 * @return TResult containing FDataTableInfo or error
	 */
	TResult<FDataTableInfo> GetDataTableInfo(UDataTable* DataTable, bool bIncludeColumns = true);

	// ========== Property Reflection Helpers ==========

	/**
	 * @brief Get a descriptive type string for a property
	 * 
	 * @param Property The property to describe
	 * @return Type string like "float", "TArray<FString>", "UTexture2D*"
	 */
	FString GetPropertyTypeString(FProperty* Property) const;

	/**
	 * @brief Check if a property should be exposed to the tool
	 * 
	 * @param Property The property to check
	 * @return true if the property should be visible/editable
	 */
	bool ShouldExposeProperty(FProperty* Property) const;
};
