// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Information about a row struct type that can be used for data tables
 */
struct VIBEUE_API FRowStructInfo
{
	/** Struct name (e.g., "FMyItemRow") */
	FString Name;
	
	/** Full path (e.g., "/Script/MyGame.FMyItemRow") */
	FString Path;
	
	/** Module/package name */
	FString Module;
	
	/** Parent struct name (usually "FTableRowBase") */
	FString ParentStruct;
	
	/** Whether this is a native C++ struct (vs Blueprint-defined) */
	bool bIsNative = true;
	
	/** List of property names on this struct */
	TArray<FString> PropertyNames;
};

/**
 * Information about a column/property in a row struct
 */
struct VIBEUE_API FColumnInfo
{
	/** Property name */
	FString Name;
	
	/** Display type string (e.g., "FString", "int32", "UTexture2D*") */
	FString Type;
	
	/** Full C++ type */
	FString CppType;
	
	/** Category from metadata */
	FString Category;
	
	/** Tooltip from metadata */
	FString Tooltip;
	
	/** Whether this property is editable */
	bool bEditable = true;
	
	/** Default value as string (if available) */
	FString DefaultValue;
};

/**
 * Information about a data table asset
 */
struct VIBEUE_API FDataTableInfo
{
	/** Asset name */
	FString Name;
	
	/** Full asset path */
	FString Path;
	
	/** Row struct name */
	FString RowStructName;
	
	/** Row struct full path */
	FString RowStructPath;
	
	/** Number of rows in the table */
	int32 RowCount = 0;
	
	/** Column definitions */
	TArray<FColumnInfo> Columns;
};

/**
 * Result of a row operation
 */
struct VIBEUE_API FRowOperationResult
{
	/** Row name that was operated on */
	FString RowName;
	
	/** Whether operation succeeded */
	bool bSuccess = false;
	
	/** Error message if failed */
	FString Error;
	
	/** Properties that were modified (for update operations) */
	TArray<FString> ModifiedProperties;
};

/**
 * Result of bulk row operations
 */
struct VIBEUE_API FBulkRowResult
{
	/** Total rows processed */
	int32 TotalCount = 0;
	
	/** Rows that succeeded */
	TArray<FString> SucceededRows;
	
	/** Rows that failed with their errors */
	TMap<FString, FString> FailedRows;
};
