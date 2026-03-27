// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UDataAssetService.generated.h"

/**
 * Information about a DataAsset class type
 */
USTRUCT(BlueprintType)
struct FDataAssetTypeInfo
{
	GENERATED_BODY()

	/** Class name (without U prefix) */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Name;

	/** Full path to the class */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Path;

	/** Module containing this class */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Module;

	/** Whether this is a native C++ class */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	bool bIsNative = true;

	/** Parent class name */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString ParentClass;
};

/**
 * Information about a DataAsset instance
 */
USTRUCT(BlueprintType)
struct FDataAssetInstanceInfo
{
	GENERATED_BODY()

	/** Asset name */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Name;

	/** Full asset path */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Path;

	/** Class name */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString ClassName;

	/** Class path */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString ClassPath;

	/** Parent class chain */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	TArray<FString> ParentClasses;

	/** All property values as JSON string */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString PropertiesJson;
};

/**
 * Information about a property on a DataAsset
 */
USTRUCT(BlueprintType)
struct FDataAssetPropertyInfo
{
	GENERATED_BODY()

	/** Property name */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Name;

	/** Property type as string */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Type;

	/** Property category from metadata */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Category;

	/** Tooltip/description */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Description;

	/** Class that defines this property */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString DefinedIn;

	/** Whether property is read-only */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	bool bReadOnly = false;

	/** Whether property is an array */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	bool bIsArray = false;

	/** Property flags as comma-separated string */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Flags;
};

/**
 * Information about a DataAsset class (schema/blueprint)
 */
USTRUCT(BlueprintType)
struct FDataAssetClassInfo
{
	GENERATED_BODY()

	/** Class name */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Name;

	/** Full class path */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	FString Path;

	/** Whether class is abstract */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	bool bIsAbstract = false;

	/** Whether class is native C++ */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	bool bIsNative = true;

	/** Parent class chain */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	TArray<FString> ParentClasses;

	/** Properties defined on this class */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	TArray<FDataAssetPropertyInfo> Properties;
};

/**
 * Result of setting multiple properties
 */
USTRUCT(BlueprintType)
struct FDataAssetSetPropertiesResult
{
	GENERATED_BODY()

	/** Properties that were successfully set */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	TArray<FString> SuccessProperties;

	/** Properties that failed to set with error messages */
	UPROPERTY(BlueprintReadOnly, Category = "DataAsset")
	TArray<FString> FailedProperties;
};

/**
 * DataAsset Service - Python API for DataAsset manipulation
 *
 * Provides 9 DataAsset management actions:
 *
 * Discovery:
 * - search_types: Find available DataAsset subclasses
 * - list: List DataAsset instances
 * - get_class_info: Get detailed class schema info
 *
 * Lifecycle:
 * - create: Create a new DataAsset instance
 *
 * Information:
 * - get_info: Get DataAsset instance info with properties
 * - list_properties: List all editable properties
 *
 * Properties:
 * - get_property: Get a single property value
 * - set_property: Set a single property value
 * - set_properties: Set multiple properties at once
 *
 * Python Usage:
 *   import unreal
 *
 *   # Search for DataAsset types
 *   types = unreal.DataAssetService.search_types("Item")
 *
 *   # Create a DataAsset
 *   result = unreal.DataAssetService.create_data_asset("ItemDataAsset", "/Game/Data", "DA_Sword")
 *
 *   # Get asset info
 *   info = unreal.DataAssetService.get_info("/Game/Data/DA_Sword")
 *
 *   # Set property
 *   unreal.DataAssetService.set_property("/Game/Data/DA_Sword", "ItemName", "Excalibur")
 *
 * @note This replaces the JSON-based manage_data_asset MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UDataAssetService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Discovery Actions
	// =================================================================

	/**
	 * Search for DataAsset subclasses.
	 * Maps to action="search_types"
	 *
	 * @param SearchFilter - Optional search term for class names
	 * @return Array of DataAsset type information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static TArray<FDataAssetTypeInfo> SearchTypes(const FString& SearchFilter = TEXT(""));

	/**
	 * List DataAsset instances.
	 * Maps to action="list"
	 *
	 * @param ClassName - Optional filter by specific DataAsset subclass
	 * @param SearchPath - Path to search in (default: /Game)
	 * @return Array of DataAsset paths
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static TArray<FString> ListDataAssets(const FString& ClassName = TEXT(""), const FString& SearchPath = TEXT("/Game"));

	/**
	 * Get detailed information about a DataAsset class (schema).
	 * Maps to action="get_class_info"
	 *
	 * @param ClassName - Class name to get info for
	 * @param bIncludeAll - Include non-editable properties
	 * @return Class information with properties
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static FDataAssetClassInfo GetClassInfo(const FString& ClassName, bool bIncludeAll = false);

	// =================================================================
	// Lifecycle Actions
	// =================================================================

	/**
	 * Create a new DataAsset instance.
	 * Maps to action="create"
	 *
	 * @param ClassName - DataAsset class to instantiate
	 * @param AssetPath - Directory to create in (default: /Game/Data)
	 * @param AssetName - Name for the new asset
	 * @param PropertiesJson - Optional initial properties as JSON object string
	 * @return Full path to created asset (empty on failure)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static FString CreateDataAsset(
		const FString& ClassName,
		const FString& AssetPath,
		const FString& AssetName,
		const FString& PropertiesJson = TEXT(""));

	// =================================================================
	// Information Actions
	// =================================================================

	/**
	 * Get comprehensive info about a DataAsset instance.
	 * Maps to action="get_info"
	 *
	 * @param AssetPath - Full path to the DataAsset
	 * @return Asset info with class and properties
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static FDataAssetInstanceInfo GetInfo(const FString& AssetPath);

	/**
	 * List all editable properties on a DataAsset.
	 * Maps to action="list_properties"
	 *
	 * @param AssetPath - Path to DataAsset or empty to use ClassName
	 * @param ClassName - Class name if no asset path provided
	 * @param bIncludeAll - Include non-editable properties
	 * @return Array of property information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static TArray<FDataAssetPropertyInfo> ListProperties(
		const FString& AssetPath = TEXT(""),
		const FString& ClassName = TEXT(""),
		bool bIncludeAll = false);

	// =================================================================
	// Property Actions
	// =================================================================

	/**
	 * Get a property value from a DataAsset.
	 * Maps to action="get_property"
	 *
	 * @param AssetPath - Full path to the DataAsset
	 * @param PropertyName - Name of the property to get
	 * @return Property value as string
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static FString GetProperty(const FString& AssetPath, const FString& PropertyName);

	/**
	 * Set a property value on a DataAsset.
	 * Maps to action="set_property"
	 *
	 * @param AssetPath - Full path to the DataAsset
	 * @param PropertyName - Name of the property to set
	 * @param PropertyValue - Value to set (as string, will be parsed)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static bool SetProperty(const FString& AssetPath, const FString& PropertyName, const FString& PropertyValue);

	/**
	 * Set multiple properties on a DataAsset at once.
	 * Maps to action="set_properties"
	 *
	 * @param AssetPath - Full path to the DataAsset
	 * @param PropertiesJson - JSON object string with property name/value pairs
	 * @return Result with success and failed property lists
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static FDataAssetSetPropertiesResult SetProperties(const FString& AssetPath, const FString& PropertiesJson);

	// =================================================================
	// Legacy Compatibility
	// =================================================================

	/**
	 * Get all property values from a DataAsset as JSON.
	 * Legacy method - prefer GetInfo() for new code.
	 *
	 * @param AssetPath - Full path to the DataAsset
	 * @return JSON string representation of the DataAsset properties
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets")
	static FString GetPropertiesAsJson(const FString& AssetPath);

	// =================================================================
	// Existence Checks
	// =================================================================

	/**
	 * Check if a DataAsset exists at the given path.
	 *
	 * @param AssetPath - Full path to the DataAsset
	 * @return True if DataAsset exists
	 *
	 * Example:
	 *   if not unreal.DataAssetService.data_asset_exists("/Game/Data/DA_Sword"):
	 *       unreal.DataAssetService.create_data_asset("ItemDataAsset", "/Game/Data", "DA_Sword")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|DataAssets|Exists")
	static bool DataAssetExists(const FString& AssetPath);

private:
	// Helper to find DataAsset class by name
	static UClass* FindDataAssetClass(const FString& ClassName);
	
	// Helper to load DataAsset by path
	static UDataAsset* LoadDataAsset(const FString& AssetPath);
	
	// Helper to check if property should be exposed
	static bool ShouldExposeProperty(FProperty* Property, bool bIncludeAll = false);
	
	// Helper to get property type as string
	static FString GetPropertyTypeString(FProperty* Property);
	
	// Helper to convert property to string value
	static FString PropertyToString(FProperty* Property, void* Container);
	
	// Helper to set property from string value
	static bool SetPropertyFromString(FProperty* Property, void* Container, const FString& Value, FString& OutError);
};
