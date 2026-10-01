// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UProjectSettingsService.generated.h"

/**
 * Result of a project settings operation.
 * Python access: result = unreal.ProjectSettingsService.set_ini_value(section, key, value, config_file)
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
 *   # Direct INI access
 *   value = unreal.ProjectSettingsService.get_ini_value(
 *       "/Script/Engine.Engine", "GameEngine", "DefaultEngine.ini")
 *   result = unreal.ProjectSettingsService.set_ini_value(
 *       "/Script/EngineSettings.GeneralProjectSettings", "ProjectName", "My Game", "DefaultGame.ini")
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
	// Settings objects
	// =================================================================
	// GConfig->SetString / Flush on a bare project path ("DefaultEditor.ini") changes nothing on disk: GConfig saves
	// only its config branches, and loads another path as a NoSave single file. The supported way to save a project
	// setting is the one the editor's Settings window uses (Developer/Settings SettingsSection.cpp Save()): change the
	// settings object's CDO with Pre/PostEditChange, then TryUpdateDefaultConfigFile() for defaultconfig classes,
	// UpdateGlobalUserConfigFile() / UpdateProjectUserConfigFile() for user config classes, SaveConfig() otherwise.
	// These two functions do exactly that.

	/**
	 * Set one config property of a settings class the way the editor's Settings window does: the value applies live
	 * (PostEditChangeProperty runs) and is saved to the class's own config file, then read back from the reloaded
	 * config to verify it.
	 *
	 * @param SettingsClass - Class path ("/Script/UnrealEd.EditorProjectAppearanceSettings") or name
	 *                        ("EditorProjectAppearanceSettings", a leading U is accepted)
	 * @param PropertyName - C++ name ("bDisplayUnits"), case-insensitive; Python style ("display_units") also works
	 * @param Value - Unreal text format: "True", "12.5", "Feet", arrays as "(Feet,Inches)", structs as "(X=1,Y=2)"
	 * @return Operation result; ModifiedSettings lists "[Section] Property: old -> new (file)"
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
	static FProjectSettingResult SetSettingsProperty(const FString& SettingsClass, const FString& PropertyName, const FString& Value);

	/**
	 * Current value of a settings class property (its CDO), in Unreal text format. Empty if not found.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
	static FString GetSettingsProperty(const FString& SettingsClass, const FString& PropertyName);

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
	 * Writes the key into the file ON DISK with Epic's FConfigFile::UpdateSinglePropertyInSection (the
	 * rest of the file, comments included, is kept), reloads that config branch so the editor's config cache sees it,
	 * and reads the file back; bSuccess is false when the value did not land. For a property of a settings class
	 * prefer SetSettingsProperty (it also applies the value live).
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
	 * An array in a layered project file needs the engine's array commands (!Key=ClearArray, +Key=...)
	 * relative to the lower layers, which only the settings-object path writes correctly. When Section is a settings
	 * class section ("/Script/Module.Class") this forwards to SetSettingsProperty (the array applies live and is saved
	 * the Settings-window way); any other section fails with an explanation instead of reporting a success that
	 * never reached the disk.
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
	 * SetIniValue / SetSettingsProperty write to disk immediately, so for a project Default*.ini
	 * there is nothing pending and this returns true; for a config branch name ("Engine", "Editor", "Game") it
	 * flushes that branch's user layer (Saved/Config).
	 *
	 * @param ConfigFile - Config file name (e.g., "DefaultEngine.ini")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|ProjectSettings")
	static bool SaveConfig(const FString& ConfigFile);
};
