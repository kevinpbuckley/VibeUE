// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/DataTable/Types/DataTableTypes.h"
#include "Core/Result.h"

// Forward declarations
class UDataTable;
class UScriptStruct;

/**
 * @class FDataTableLifecycleService
 * @brief Service for creating data table assets
 * 
 * This service provides functionality for:
 * - Creating new data tables with specified row structs
 * 
 * For delete/duplicate/save operations, use the Asset tools (manage_asset).
 * 
 * @see FServiceBase
 * @see TResult
 */
class VIBEUE_API FDataTableLifecycleService : public FServiceBase
{
public:
	/**
	 * @brief Constructor
	 * @param Context Service context for shared state (can be nullptr)
	 */
	explicit FDataTableLifecycleService(TSharedPtr<FServiceContext> Context);

	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("DataTableLifecycleService"); }

	/**
	 * @brief Create a new data table asset
	 * 
	 * @param RowStruct The row struct type for this table
	 * @param AssetPath Destination folder (e.g., "/Game/Data")
	 * @param AssetName Name for the new data table (e.g., "DT_Items")
	 * @return TResult containing the created UDataTable or error
	 */
	TResult<UDataTable*> CreateDataTable(
		UScriptStruct* RowStruct,
		const FString& AssetPath,
		const FString& AssetName);

	/**
	 * @brief Create a new data table using struct name
	 * 
	 * @param RowStructName Row struct name or path (e.g., "FMyItemRow")
	 * @param AssetPath Destination folder
	 * @param AssetName Name for the new data table
	 * @return TResult containing the created UDataTable or error
	 */
	TResult<UDataTable*> CreateDataTableByStructName(
		const FString& RowStructName,
		const FString& AssetPath,
		const FString& AssetName);
};
