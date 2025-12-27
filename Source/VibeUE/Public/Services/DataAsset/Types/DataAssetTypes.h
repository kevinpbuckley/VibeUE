// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Information about a data asset class type
 */
struct FDataAssetTypeInfo
{
	/** Class name (without U prefix) */
	FString Name;
	
	/** Full path to the class */
	FString Path;
	
	/** Module containing this class */
	FString Module;
	
	/** Whether this is a native C++ class */
	bool bIsNative = true;
	
	/** Parent class name */
	FString ParentClass;
};

/**
 * Information about a data asset instance
 */
struct FDataAssetInfo
{
	/** Asset name */
	FString Name;
	
	/** Full asset path */
	FString Path;
	
	/** Class name */
	FString ClassName;
	
	/** Class path */
	FString ClassPath;
	
	/** Parent class chain */
	TArray<FString> ParentClasses;
};

/**
 * Information about a property on a data asset
 */
struct FDataAssetPropertyInfo
{
	/** Property name */
	FString Name;
	
	/** Property type as string */
	FString Type;
	
	/** Property category from metadata */
	FString Category;
	
	/** Tooltip/description */
	FString Description;
	
	/** Class that defines this property */
	FString DefinedIn;
	
	/** Whether property is read-only */
	bool bReadOnly = false;
	
	/** Whether property is an array */
	bool bIsArray = false;
	
	/** Property flags as comma-separated string */
	FString Flags;
};

/**
 * Result of a data asset creation operation
 */
struct FDataAssetCreateResult
{
	/** Full path to created asset */
	FString AssetPath;
	
	/** Asset name */
	FString AssetName;
	
	/** Class name */
	FString ClassName;
};

/**
 * Result of setting properties on a data asset
 */
struct FSetPropertiesResult
{
	/** Properties that were successfully set */
	TArray<FString> SuccessProperties;
	
	/** Properties that failed with error messages */
	TArray<FString> FailedProperties;
};
