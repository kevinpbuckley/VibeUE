// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UEnumStructService.generated.h"

// ============================================================================
// ENUM STRUCTURES
// ============================================================================

/**
 * Information about a single enum value
 */
USTRUCT(BlueprintType)
struct FEnumValueInfo
{
	GENERATED_BODY()

	/** Internal name of the enum value */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	FString Name;

	/** Display name shown in the editor */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	FString DisplayName;

	/** Description/tooltip for this value */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	FString Description;

	/** Numeric value of this enum entry */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	int64 Value = 0;

	/** Index position in the enum */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	int32 Index = 0;
};

/**
 * Detailed information about an enum
 */
USTRUCT(BlueprintType)
struct FEnumInfo
{
	GENERATED_BODY()

	/** Enum name without path */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	FString Name;

	/** Full asset path */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	FString Path;

	/** Whether this is a UserDefinedEnum (editable) or native C++ enum (read-only) */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	bool bIsUserDefined = false;

	/** Whether this enum uses bitflags */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	bool bIsBitFlags = false;

	/** Module/plugin containing this enum (for native enums) */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	FString Module;

	/** Number of values (excluding _MAX) */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	int32 ValueCount = 0;

	/** All enum values */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	TArray<FEnumValueInfo> Values;
};

/**
 * Search result for enum discovery
 */
USTRUCT(BlueprintType)
struct FEnumSearchResult
{
	GENERATED_BODY()

	/** Enum name */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	FString Name;

	/** Full asset path */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	FString Path;

	/** Whether this is a UserDefinedEnum */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	bool bIsUserDefined = false;

	/** Number of values */
	UPROPERTY(BlueprintReadOnly, Category = "Enum")
	int32 ValueCount = 0;
};

// ============================================================================
// STRUCT STRUCTURES
// ============================================================================

/**
 * Information about a single struct property
 */
USTRUCT(BlueprintType)
struct FStructPropertyInfo
{
	GENERATED_BODY()

	/** Property name */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString Name;

	/** Property type as friendly string (e.g., "float", "FVector", "TArray<int32>") */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString Type;

	/** Full CPP type path */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString TypePath;

	/** Category for organization */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString Category;

	/** Tooltip/description */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString Description;

	/** Default value as string */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString DefaultValue;

	/** GUID for UserDefinedStruct properties */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString Guid;

	/** Property index/order */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	int32 Index = 0;

	/** Whether this is an array container */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	bool bIsArray = false;

	/** Whether this is a map container */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	bool bIsMap = false;

	/** Whether this is a set container */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	bool bIsSet = false;
};

/**
 * Detailed information about a struct
 */
USTRUCT(BlueprintType)
struct FStructInfo
{
	GENERATED_BODY()

	/** Struct name without path */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString Name;

	/** Full asset path */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString Path;

	/** Whether this is a UserDefinedStruct (editable) or native C++ struct (read-only) */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	bool bIsUserDefined = false;

	/** Parent struct name (if any) */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString ParentStruct;

	/** Module/plugin containing this struct */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString Module;

	/** Structure size in bytes */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	int32 StructureSize = 0;

	/** Number of properties */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	int32 PropertyCount = 0;

	/** All properties */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	TArray<FStructPropertyInfo> Properties;
};

/**
 * Search result for struct discovery
 */
USTRUCT(BlueprintType)
struct FStructSearchResult
{
	GENERATED_BODY()

	/** Struct name */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString Name;

	/** Full asset path */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	FString Path;

	/** Whether this is a UserDefinedStruct */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	bool bIsUserDefined = false;

	/** Number of properties */
	UPROPERTY(BlueprintReadOnly, Category = "Struct")
	int32 PropertyCount = 0;
};

// ============================================================================
// SERVICE CLASS
// ============================================================================

/**
 * Enum and Struct Service - Python API for UserDefinedEnum and UserDefinedStruct manipulation
 *
 * Provides full CRUD operations for:
 * - UserDefinedEnum: Create, read, modify, delete enum assets
 * - UserDefinedStruct: Create, read, modify, delete struct assets
 *
 * Note: Native C++ enums and structs are read-only. Modification methods will
 * return false/error for native types.
 *
 * Python Usage:
 *   import unreal
 *
 *   # Search for enums
 *   enums = unreal.EnumStructService.search_enums("Weapon")
 *
 *   # Create a UserDefinedEnum
 *   path = unreal.EnumStructService.create_enum("/Game/Enums", "EWeaponType")
 *   unreal.EnumStructService.add_enum_value(path, "Sword", "A melee weapon")
 *
 *   # Create a UserDefinedStruct
 *   path = unreal.EnumStructService.create_struct("/Game/Structs", "FWeaponData")
 *   unreal.EnumStructService.add_struct_property(path, "Damage", "float")
 */
UCLASS(BlueprintType)
class VIBEUE_API UEnumStructService : public UObject
{
	GENERATED_BODY()

public:
	// ========================================================================
	// ENUM DISCOVERY
	// ========================================================================

	/**
	 * Search for enums matching a pattern.
	 * Returns both UserDefinedEnums and native C++ enums.
	 *
	 * @param SearchFilter - Partial name match (case-insensitive). Empty returns all.
	 * @param bUserDefinedOnly - If true, only return UserDefinedEnums
	 * @param MaxResults - Maximum results to return (0 = unlimited)
	 * @return Array of enum search results
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static TArray<FEnumSearchResult> SearchEnums(
		const FString& SearchFilter = TEXT(""),
		bool bUserDefinedOnly = false,
		int32 MaxResults = 50);

	/**
	 * Get detailed information about an enum.
	 *
	 * @param EnumPathOrName - Full path or enum name
	 * @param OutInfo - Populated enum information
	 * @return True if enum was found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool GetEnumInfo(const FString& EnumPathOrName, FEnumInfo& OutInfo);

	/**
	 * Get all values from an enum as a simple string array.
	 *
	 * @param EnumPathOrName - Full path or enum name
	 * @return Array of enum value names (empty if enum not found)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static TArray<FString> GetEnumValues(const FString& EnumPathOrName);

	// ========================================================================
	// ENUM LIFECYCLE
	// ========================================================================

	/**
	 * Create a new UserDefinedEnum asset.
	 *
	 * @param AssetPath - Directory to create in (e.g., "/Game/Enums")
	 * @param EnumName - Name for the enum (will add E prefix if not present)
	 * @return Full path to created asset (empty on failure)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static FString CreateEnum(const FString& AssetPath, const FString& EnumName);

	/**
	 * Delete a UserDefinedEnum asset.
	 *
	 * @param EnumPath - Full path to the enum asset
	 * @return True if deleted successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool DeleteEnum(const FString& EnumPath);

	// ========================================================================
	// ENUM VALUE OPERATIONS
	// ========================================================================

	/**
	 * Add a new value to a UserDefinedEnum.
	 *
	 * @param EnumPath - Full path to the UserDefinedEnum
	 * @param ValueName - Name for the new value
	 * @param DisplayName - Display name (optional, defaults to ValueName)
	 * @return True if value was added
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool AddEnumValue(
		const FString& EnumPath,
		const FString& ValueName,
		const FString& DisplayName = TEXT(""));

	/**
	 * Remove a value from a UserDefinedEnum.
	 *
	 * @param EnumPath - Full path to the UserDefinedEnum
	 * @param ValueName - Name of the value to remove
	 * @return True if value was removed
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool RemoveEnumValue(const FString& EnumPath, const FString& ValueName);

	/**
	 * Rename an enum value in a UserDefinedEnum.
	 *
	 * @param EnumPath - Full path to the UserDefinedEnum
	 * @param OldValueName - Current value name
	 * @param NewValueName - New value name
	 * @return True if renamed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool RenameEnumValue(
		const FString& EnumPath,
		const FString& OldValueName,
		const FString& NewValueName);

	/**
	 * Set the display name for an enum value.
	 *
	 * @param EnumPath - Full path to the UserDefinedEnum
	 * @param ValueName - Name of the value
	 * @param DisplayName - New display name
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool SetEnumValueDisplayName(
		const FString& EnumPath,
		const FString& ValueName,
		const FString& DisplayName);

	// ========================================================================
	// STRUCT DISCOVERY
	// ========================================================================

	/**
	 * Search for structs matching a pattern.
	 * Returns both UserDefinedStructs and native C++ structs.
	 *
	 * @param SearchFilter - Partial name match (case-insensitive). Empty returns all.
	 * @param bUserDefinedOnly - If true, only return UserDefinedStructs
	 * @param MaxResults - Maximum results to return (0 = unlimited)
	 * @return Array of struct search results
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static TArray<FStructSearchResult> SearchStructs(
		const FString& SearchFilter = TEXT(""),
		bool bUserDefinedOnly = false,
		int32 MaxResults = 50);

	/**
	 * Get detailed information about a struct.
	 *
	 * @param StructPathOrName - Full path or struct name
	 * @param OutInfo - Populated struct information
	 * @return True if struct was found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool GetStructInfo(const FString& StructPathOrName, FStructInfo& OutInfo);

	// ========================================================================
	// STRUCT LIFECYCLE
	// ========================================================================

	/**
	 * Create a new UserDefinedStruct asset.
	 *
	 * @param AssetPath - Directory to create in (e.g., "/Game/Structs")
	 * @param StructName - Name for the struct (will add F prefix if not present)
	 * @return Full path to created asset (empty on failure)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static FString CreateStruct(const FString& AssetPath, const FString& StructName);

	/**
	 * Delete a UserDefinedStruct asset.
	 *
	 * @param StructPath - Full path to the struct asset
	 * @return True if deleted successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool DeleteStruct(const FString& StructPath);

	// ========================================================================
	// STRUCT PROPERTY OPERATIONS
	// ========================================================================

	/**
	 * Add a property to a UserDefinedStruct.
	 *
	 * @param StructPath - Full path to the UserDefinedStruct
	 * @param PropertyName - Name for the property
	 * @param PropertyType - Type string (e.g., "float", "FVector", "AActor", "EMyEnum")
	 * @param DefaultValue - Default value as string (optional)
	 * @param ContainerType - "Array", "Set", or "Map" (empty for single value)
	 * @return True if property was added
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool AddStructProperty(
		const FString& StructPath,
		const FString& PropertyName,
		const FString& PropertyType,
		const FString& DefaultValue = TEXT(""),
		const FString& ContainerType = TEXT(""));

	/**
	 * Remove a property from a UserDefinedStruct.
	 *
	 * @param StructPath - Full path to the UserDefinedStruct
	 * @param PropertyName - Name of the property to remove
	 * @return True if property was removed
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool RemoveStructProperty(const FString& StructPath, const FString& PropertyName);

	/**
	 * Rename a property in a UserDefinedStruct.
	 *
	 * @param StructPath - Full path to the UserDefinedStruct
	 * @param OldPropertyName - Current property name
	 * @param NewPropertyName - New property name
	 * @return True if renamed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool RenameStructProperty(
		const FString& StructPath,
		const FString& OldPropertyName,
		const FString& NewPropertyName);

	/**
	 * Change a property's type in a UserDefinedStruct.
	 * WARNING: This may cause data loss for existing instances.
	 *
	 * @param StructPath - Full path to the UserDefinedStruct
	 * @param PropertyName - Name of the property
	 * @param NewPropertyType - New type string
	 * @return True if type was changed
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool ChangeStructPropertyType(
		const FString& StructPath,
		const FString& PropertyName,
		const FString& NewPropertyType);

	/**
	 * Set a property's default value in a UserDefinedStruct.
	 *
	 * @param StructPath - Full path to the UserDefinedStruct
	 * @param PropertyName - Name of the property
	 * @param DefaultValue - New default value as string
	 * @return True if default was set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool SetStructPropertyDefault(
		const FString& StructPath,
		const FString& PropertyName,
		const FString& DefaultValue);

	// ========================================================================
	// EXISTENCE CHECKS
	// ========================================================================

	/**
	 * Check if an enum exists.
	 *
	 * @param EnumPathOrName - Full path or enum name
	 * @return True if enum exists
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool EnumExists(const FString& EnumPathOrName);

	/**
	 * Check if a struct exists.
	 *
	 * @param StructPathOrName - Full path or struct name
	 * @return True if struct exists
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EnumStruct")
	static bool StructExists(const FString& StructPathOrName);

private:
	// Helper to load a UserDefinedEnum by path or name
	static class UUserDefinedEnum* LoadUserDefinedEnum(const FString& EnumPathOrName);

	// Helper to find any enum by path or name (including native)
	static UEnum* FindEnum(const FString& EnumPathOrName);

	// Helper to load a UserDefinedStruct by path or name
	static class UUserDefinedStruct* LoadUserDefinedStruct(const FString& StructPathOrName);

	// Helper to find any struct by path or name (including native)
	static UScriptStruct* FindStruct(const FString& StructPathOrName);

	// Helper to get property type string
	static FString GetPropertyTypeString(FProperty* Property);

	// Helper to find enum value index by name
	static int32 FindEnumValueIndex(UEnum* Enum, const FString& ValueName);

	// Helper to get property GUID in UserDefinedStruct
	static FGuid FindPropertyGuid(class UUserDefinedStruct* Struct, const FString& PropertyName);
};
