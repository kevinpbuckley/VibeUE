// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UProjectSettingsService.generated.h"

/**
 * Information about a single project setting.
 * Python access: info = unreal.ProjectSettingsService.get_setting_info(category, key)
 *
 * Properties:
 * - key (str): Setting key name (e.g., "ProjectName", "bGlobalGravitySet")
 * - display_name (str): Human-readable display name
 * - description (str): Tooltip/description text
 * - type (str): Setting type: "string", "int", "float", "bool", "array", "object"
 * - value (str): Current value as string (JSON-encoded for complex types)
 * - default_value (str): Default value as string
 * - config_section (str): INI section path (e.g., "/Script/EngineSettings.GeneralProjectSettings")
 * - config_file (str): INI file name (e.g., "DefaultGame.ini")
 * - requires_restart (bool): Whether this setting requires editor restart
 * - read_only (bool): Whether this setting cannot be modified
 */
USTRUCT(BlueprintType)
struct FProjectSettingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString Key;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString Type;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString Value;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString DefaultValue;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString ConfigSection;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString ConfigFile;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	bool bRequiresRestart = false;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	bool bReadOnly = false;
};

/**
 * Information about a settings category.
 * Python access: categories = unreal.ProjectSettingsService.list_categories()
 *
 * Properties:
 * - category_id (str): Category identifier (e.g., "general", "maps", "rendering")
 * - display_name (str): Human-readable category name
 * - description (str): Category description
 * - setting_count (int): Number of settings in this category
 * - settings_class_name (str): Associated UObject settings class path
 * - config_file (str): Primary config file for this category
 */
USTRUCT(BlueprintType)
struct FProjectSettingCategory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString CategoryId;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	int32 SettingCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString SettingsClassName;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString ConfigFile;
};

/**
 * Result of a project settings operation.
 * Python access: result = unreal.ProjectSettingsService.set_setting(category, key, value)
 *
 * Properties:
 * - success (bool): Whether the operation succeeded
 * - error_message (str): Error message if failed (empty if success)
 * - modified_settings (Array[str]): Settings that were successfully modified
 * - failed_settings (Array[str]): Settings that failed to modify with reasons
 * - requires_restart (bool): Whether changes require editor restart
 */
USTRUCT(BlueprintType)
struct FProjectSettingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	TArray<FString> ModifiedSettings;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	TArray<FString> FailedSettings;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	bool bRequiresRestart = false;
};

/**
 * Information about a discovered settings class.
 * Python access: classes = unreal.ProjectSettingsService.discover_settings_classes()
 *
 * Properties:
 * - class_name (str): UClass name (e.g., "UGeneralProjectSettings")
 * - class_path (str): Full class path
 * - config_section (str): INI section this class maps to
 * - config_file (str): INI file this class saves to
 * - property_count (int): Number of configurable properties
 * - is_developer_settings (bool): Whether this is a UDeveloperSettings subclass
 */
USTRUCT(BlueprintType)
struct FSettingsClassInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString ClassName;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString ClassPath;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString ConfigSection;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	FString ConfigFile;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	int32 PropertyCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "ProjectSettings")
	bool bIsDeveloperSettings = false;
};

/**
 * Project Settings Service - Python API for Unreal Engine project settings manipulation.
 *
 * Provides comprehensive access to project configuration including:
 * - General project settings (name, company, legal)
 * - Map settings (default maps, game modes)
 * - Rendering settings (quality, features)
 * - Physics settings (gravity, collision)
 * - Input settings
 * - Audio settings
 * - All UDeveloperSettings subclasses (dynamically discovered)
 * - Direct INI file access for custom sections
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all categories
 *   categories = unreal.ProjectSettingsService.list_categories()
 *   for cat in categories:
 *       print(f"{cat.category_id}: {cat.display_name} ({cat.setting_count} settings)")
 *
 *   # Get all settings in a category
 *   settings = unreal.ProjectSettingsService.list_settings("general")
 *   for s in settings:
 *       print(f"  {s.key} = {s.value}")
 *
 *   # Get a specific setting
 *   value = unreal.ProjectSettingsService.get_setting("general", "ProjectName")
 *   print(f"Project name: {value}")
 *
 *   # Set a setting
 *   result = unreal.ProjectSettingsService.set_setting("general", "ProjectName", "My Game")
 *   if result.success:
 *       print("Setting updated!")
 *
 *   # Direct INI access
 *   value = unreal.ProjectSettingsService.get_ini_value(
 *       "/Script/Engine.Engine", "GameEngine", "DefaultEngine.ini")
 *
 *   # Discover all settings classes
 *   classes = unreal.ProjectSettingsService.discover_settings_classes()
 *   for c in classes:
 *       print(f"{c.class_name}: {c.property_count} properties")
 *
 * @note Changes are written to config files and may require editor restart
 */
UCLASS(BlueprintType)
class VIBEUE_API UProjectSettingsService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Category Operations
	// =================================================================

	/**
	 * List all available settings categories.
	 * Includes both predefined categories and dynamically discovered UDeveloperSettings.
	 *
	 * @return Array of category information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static TArray<FProjectSettingCategory> ListCategories();

	// =================================================================
	// Settings Discovery
	// =================================================================

	/**
	 * Discover all UDeveloperSettings subclasses in the engine and project.
	 * Returns settings classes that can be configured via Project Settings UI.
	 *
	 * @return Array of settings class information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static TArray<FSettingsClassInfo> DiscoverSettingsClasses();

	/**
	 * List all settings within a category with their current values.
	 *
	 * @param CategoryId - Category identifier (e.g., "general", "maps", "rendering")
	 * @return Array of setting information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static TArray<FProjectSettingInfo> ListSettings(const FString& CategoryId);

	/**
	 * Get detailed information about a specific setting.
	 *
	 * @param CategoryId - Category identifier
	 * @param Key - Setting key name
	 * @param OutInfo - Output structure with setting details
	 * @return True if setting was found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static bool GetSettingInfo(const FString& CategoryId, const FString& Key, FProjectSettingInfo& OutInfo);

	// =================================================================
	// Get/Set Individual Settings
	// =================================================================

	/**
	 * Get a single setting value.
	 *
	 * @param CategoryId - Category identifier
	 * @param Key - Setting key name
	 * @return Setting value as string (empty if not found)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static FString GetSetting(const FString& CategoryId, const FString& Key);

	/**
	 * Set a single setting value.
	 *
	 * @param CategoryId - Category identifier
	 * @param Key - Setting key name
	 * @param Value - New value as string
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static FProjectSettingResult SetSetting(const FString& CategoryId, const FString& Key, const FString& Value);

	// =================================================================
	// Batch Operations
	// =================================================================

	/**
	 * Get all settings in a category as a JSON object.
	 *
	 * @param CategoryId - Category identifier
	 * @return JSON object string with all key-value pairs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static FString GetCategorySettingsAsJson(const FString& CategoryId);

	/**
	 * Set multiple settings in a category from a JSON object.
	 *
	 * @param CategoryId - Category identifier
	 * @param SettingsJson - JSON object string with key-value pairs
	 * @return Operation result with success/failure lists
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static FProjectSettingResult SetCategorySettingsFromJson(const FString& CategoryId, const FString& SettingsJson);

	// =================================================================
	// Direct INI Access
	// =================================================================

	/**
	 * List all sections in a config file.
	 *
	 * @param ConfigFile - Config file name (e.g., "DefaultEngine.ini", "DefaultGame.ini")
	 * @return Array of section names
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static TArray<FString> ListIniSections(const FString& ConfigFile);

	/**
	 * List all keys in a config section.
	 *
	 * @param Section - INI section (e.g., "/Script/Engine.Engine")
	 * @param ConfigFile - Config file name
	 * @return Array of key names
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static TArray<FString> ListIniKeys(const FString& Section, const FString& ConfigFile);

	/**
	 * Get a value directly from an INI config file.
	 *
	 * @param Section - INI section (e.g., "/Script/Engine.Engine")
	 * @param Key - Key name within the section
	 * @param ConfigFile - Config file name (e.g., "DefaultEngine.ini")
	 * @return Value as string (empty if not found)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static FString GetIniValue(const FString& Section, const FString& Key, const FString& ConfigFile);

	/**
	 * Set a value directly in an INI config file.
	 *
	 * @param Section - INI section
	 * @param Key - Key name within the section
	 * @param Value - Value to set
	 * @param ConfigFile - Config file name
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static FProjectSettingResult SetIniValue(const FString& Section, const FString& Key, const FString& Value, const FString& ConfigFile);

	/**
	 * Get an array of values from an INI config file.
	 * Some INI keys have multiple values (e.g., +ActiveGameNameRedirects).
	 *
	 * @param Section - INI section
	 * @param Key - Key name within the section
	 * @param ConfigFile - Config file name
	 * @return Array of values
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static TArray<FString> GetIniArray(const FString& Section, const FString& Key, const FString& ConfigFile);

	/**
	 * Set an array of values in an INI config file.
	 *
	 * @param Section - INI section
	 * @param Key - Key name within the section
	 * @param Values - Array of values to set
	 * @param ConfigFile - Config file name
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static FProjectSettingResult SetIniArray(const FString& Section, const FString& Key, const TArray<FString>& Values, const FString& ConfigFile);

	// =================================================================
	// Persistence
	// =================================================================

	/**
	 * Force save all pending config changes to disk.
	 *
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static bool SaveAllConfig();

	/**
	 * Save a specific config file.
	 *
	 * @param ConfigFile - Config file name (e.g., "DefaultEngine.ini")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|ProjectSettings")
	static bool SaveConfig(const FString& ConfigFile);

private:
	/** Resolve config file name to full path */
	static FString GetConfigFilePath(const FString& ConfigFile);

	/** Get the settings object for a category (may return nullptr for custom category) */
	static UObject* GetSettingsObjectForCategory(const FString& CategoryId);

	/** Get the config section for a category */
	static FString GetConfigSectionForCategory(const FString& CategoryId);

	/** Get the config file for a category */
	static FString GetConfigFileForCategory(const FString& CategoryId);

	/** Convert a property value to string */
	static FString PropertyToString(FProperty* Property, const void* Container);

	/** Set a property value from string */
	static bool StringToProperty(FProperty* Property, void* Container, const FString& Value, FString& OutError);

	/** Get the type name for a property */
	static FString GetPropertyType(FProperty* Property);

	/** Validate category ID */
	static bool ValidateCategoryId(const FString& CategoryId, FString& OutError);
};
