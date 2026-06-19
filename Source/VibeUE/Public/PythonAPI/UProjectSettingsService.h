// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UProjectSettingsService.generated.h"

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
class VIBEUE_API UProjectSettingsService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =================================================================
	// Settings Discovery
	// =================================================================

	/**
	 * Discover all UDeveloperSettings subclasses in the engine and project.
	 * Returns settings classes that can be configured via Project Settings UI.
	 *
	 * @return Array of settings class information
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
	static TArray<FSettingsClassInfo> DiscoverSettingsClasses();

	// =================================================================
	// Direct INI Access
	// =================================================================

	/**
	 * List all sections in a config file.
	 *
	 * @param ConfigFile - Config file name (e.g., "DefaultEngine.ini", "DefaultGame.ini")
	 * @return Array of section names
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
	static TArray<FString> ListIniSections(const FString& ConfigFile);

	/**
	 * List all keys in a config section.
	 *
	 * @param Section - INI section (e.g., "/Script/Engine.Engine")
	 * @param ConfigFile - Config file name
	 * @return Array of key names
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
	static TArray<FString> ListIniKeys(const FString& Section, const FString& ConfigFile);

	/**
	 * Get a value directly from an INI config file.
	 *
	 * @param Section - INI section (e.g., "/Script/Engine.Engine")
	 * @param Key - Key name within the section
	 * @param ConfigFile - Config file name (e.g., "DefaultEngine.ini")
	 * @return Value as string (empty if not found)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
	static FProjectSettingResult SetIniArray(const FString& Section, const FString& Key, const TArray<FString>& Values, const FString& ConfigFile);

	// =================================================================
	// Persistence
	// =================================================================

	/**
	 * Force save all pending config changes to disk.
	 *
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
	static bool SaveAllConfig();

	/**
	 * Save a specific config file.
	 *
	 * @param ConfigFile - Config file name (e.g., "DefaultEngine.ini")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
	static bool SaveConfig(const FString& ConfigFile);
};
