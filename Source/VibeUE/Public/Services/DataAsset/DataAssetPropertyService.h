// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Core/TResult.h"
#include "Services/DataAsset/Types/DataAssetTypes.h"

// Forward declarations
class FServiceContext;
class UDataAsset;
class UClass;

/**
 * Service for property reflection on data assets
 * 
 * Provides functionality to:
 * - Get complete info about data assets and their properties
 * - List properties on assets or classes
 * - Read and write property values
 * - Serialize properties to/from JSON
 */
class VIBEUE_API FDataAssetPropertyService
{
public:
	/**
	 * Constructor
	 * @param InServiceContext Shared service context
	 */
	explicit FDataAssetPropertyService(TSharedPtr<FServiceContext> InServiceContext);

	// ========== Asset Information ==========

	/**
	 * @brief Get complete information about a data asset
	 * 
	 * Returns asset metadata, class info, parent chain, and all property values.
	 * 
	 * @param DataAsset The data asset to examine
	 * @return TResult containing JSON object with complete info or error
	 */
	TResult<TSharedPtr<FJsonObject>> GetAssetInfo(UDataAsset* DataAsset);

	/**
	 * @brief Get information about a data asset class (without an instance)
	 * 
	 * @param AssetClass The class to examine
	 * @param bIncludeAll Include all properties, not just editable ones
	 * @return TResult containing JSON object with class info or error
	 */
	TResult<TSharedPtr<FJsonObject>> GetClassInfo(UClass* AssetClass, bool bIncludeAll = false);

	// ========== Property Listing ==========

	/**
	 * @brief List all properties on a data asset or class
	 * 
	 * @param AssetClass The class to list properties for
	 * @param bIncludeAll Include all properties, not just editable ones
	 * @return TResult containing array of FDataAssetPropertyInfo or error
	 */
	TResult<TArray<FDataAssetPropertyInfo>> ListProperties(UClass* AssetClass, bool bIncludeAll = false);

	// ========== Property Access ==========

	/**
	 * @brief Get a specific property value from a data asset
	 * 
	 * @param DataAsset The data asset to read from
	 * @param PropertyName Name of the property to read
	 * @return TResult containing JSON value or error
	 */
	TResult<TSharedPtr<FJsonValue>> GetProperty(UDataAsset* DataAsset, const FString& PropertyName);

	/**
	 * @brief Set a property value on a data asset
	 * 
	 * @param DataAsset The data asset to modify
	 * @param PropertyName Name of the property to set
	 * @param Value JSON value to set
	 * @return TResult with success or error
	 */
	TResult<void> SetProperty(UDataAsset* DataAsset, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value);

	/**
	 * @brief Set multiple properties on a data asset
	 * 
	 * @param DataAsset The data asset to modify
	 * @param Properties JSON object with property name/value pairs
	 * @return TResult containing FSetPropertiesResult or error
	 */
	TResult<FSetPropertiesResult> SetProperties(UDataAsset* DataAsset, const TSharedPtr<FJsonObject>& Properties);

	// ========== Serialization Helpers ==========

	/**
	 * @brief Serialize a property to JSON value
	 * 
	 * Handles all property types: primitives, strings, enums, objects,
	 * soft references, arrays, structs, and maps.
	 * 
	 * @param Property The property to serialize
	 * @param Container Pointer to the object containing the property
	 * @return JSON value representation
	 */
	TSharedPtr<FJsonValue> PropertyToJson(FProperty* Property, void* Container);

	/**
	 * @brief Set a property from a JSON value
	 * 
	 * @param Property The property to set
	 * @param Container Pointer to the object containing the property
	 * @param Value JSON value to set
	 * @param OutError Error message if failed
	 * @return true if successful
	 */
	bool JsonToProperty(FProperty* Property, void* Container, const TSharedPtr<FJsonValue>& Value, FString& OutError);

	/**
	 * @brief Get a human-readable type string for a property
	 * 
	 * @param Property The property to describe
	 * @return Type string like "int32", "TArray<FString>", etc.
	 */
	FString GetPropertyTypeString(FProperty* Property);

	/**
	 * @brief Check if a property should be exposed for editing
	 * 
	 * @param Property The property to check
	 * @param bIncludeAll If true, include non-editable properties
	 * @return true if property should be exposed
	 */
	bool ShouldExposeProperty(FProperty* Property, bool bIncludeAll = false);

private:
	/** Shared service context */
	TSharedPtr<FServiceContext> ServiceContext;
};
