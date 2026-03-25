// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UEngineSettingsService.generated.h"

/**
 * Information about a single engine setting.
 * Python access: info = unreal.EngineSettingsService.get_setting_info(category, key)
 *
 * Properties:
 * - key (str): Setting key name (e.g., "r.ReflectionMethod", "gc.TimeBetweenPurgingPendingKillObjects")
 * - display_name (str): Human-readable display name
 * - description (str): Tooltip/description text
 * - type (str): Setting type: "string", "int", "float", "bool", "array", "object"
 * - value (str): Current value as string (JSON-encoded for complex types)
 * - default_value (str): Default value as string
 * - config_section (str): INI section path (e.g., "/Script/Engine.RendererSettings")
 * - config_file (str): INI file name (e.g., "DefaultEngine.ini")
 * - requires_restart (bool): Whether this setting requires editor restart
 * - read_only (bool): Whether this setting cannot be modified
 * - is_console_variable (bool): Whether this is a console variable (cvar)
 */
USTRUCT(BlueprintType)
struct FEngineSettingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString Key;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString Type;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString Value;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString DefaultValue;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString ConfigSection;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString ConfigFile;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	bool bRequiresRestart = false;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	bool bReadOnly = false;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	bool bIsConsoleVariable = false;
};

/**
 * Information about an engine settings category.
 * Python access: categories = unreal.EngineSettingsService.list_categories()
 *
 * Properties:
 * - category_id (str): Category identifier (e.g., "rendering", "physics", "audio", "gc")
 * - display_name (str): Human-readable category name
 * - description (str): Category description
 * - setting_count (int): Number of settings in this category
 * - settings_class_name (str): Associated UObject settings class path
 * - config_file (str): Primary config file for this category
 */
USTRUCT(BlueprintType)
struct FEngineSettingCategory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString CategoryId;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	int32 SettingCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString SettingsClassName;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString ConfigFile;
};

/**
 * Result of an engine settings operation.
 * Python access: result = unreal.EngineSettingsService.set_setting(category, key, value)
 *
 * Properties:
 * - success (bool): Whether the operation succeeded
 * - error_message (str): Error message if failed (empty if success)
 * - modified_settings (Array[str]): Settings that were successfully modified
 * - failed_settings (Array[str]): Settings that failed to modify with reasons
 * - requires_restart (bool): Whether changes require editor restart
 */
USTRUCT(BlueprintType)
struct FEngineSettingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	TArray<FString> ModifiedSettings;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	TArray<FString> FailedSettings;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	bool bRequiresRestart = false;
};

/**
 * Information about a console variable (cvar).
 * Python access: cvar = unreal.EngineSettingsService.get_console_variable_info(name)
 *
 * Properties:
 * - name (str): Console variable name (e.g., "r.ReflectionMethod")
 * - value (str): Current value as string
 * - default_value (str): Default value as string
 * - description (str): Help text/description
 * - type (str): Value type: "int", "float", "string", "bool"
 * - flags (str): CVar flags (e.g., "ECVF_RenderThreadSafe")
 * - is_read_only (bool): Whether the cvar is read-only
 */
USTRUCT(BlueprintType)
struct FConsoleVariableInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString Value;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString DefaultValue;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString Type;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString Flags;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	bool bIsReadOnly = false;
};

/**
 * Engine Settings Service - Python API for Unreal Engine configuration manipulation.
 *
 * Provides comprehensive access to engine configuration including:
 * - Rendering settings (r.* cvars, quality, features)
 * - Physics settings (gravity, collision profiles)
 * - Audio settings (sample rates, channels)
 * - Garbage collection settings (gc.*)
 * - Threading/performance settings
 * - Platform-specific settings
 * - Console variables (cvars) direct access
 * - All engine config file access
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all categories
 *   categories = unreal.EngineSettingsService.list_categories()
 *   for cat in categories:
 *       print(f"{cat.category_id}: {cat.display_name} ({cat.setting_count} settings)")
 *
 *   # Get all settings in a category
 *   settings = unreal.EngineSettingsService.list_settings("rendering")
 *   for s in settings:
 *       print(f"  {s.key} = {s.value}")
 *
 *   # Get/set console variables
 *   value = unreal.EngineSettingsService.get_console_variable("r.ReflectionMethod")
 *   result = unreal.EngineSettingsService.set_console_variable("r.ReflectionMethod", "1")
 *
 *   # Search for settings/cvars
 *   matches = unreal.EngineSettingsService.search_console_variables("shadow")
 *   for m in matches:
 *       print(f"{m.name}: {m.description}")
 *
 * @note Changes are written to config files and may require editor restart
 */
UCLASS(BlueprintType)
class VIBEUE_API UEngineSettingsService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Category Operations
	// =================================================================

	/**
	 * List all available engine settings categories.
	 * Includes rendering, physics, audio, GC, networking, platform settings, etc.
	 *
	 * @return Array of category information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static TArray<FEngineSettingCategory> ListCategories();

	// =================================================================
	// Settings Discovery
	// =================================================================

	/**
	 * List all settings within a category with their current values.
	 *
	 * @param CategoryId - Category identifier (e.g., "rendering", "physics", "audio")
	 * @return Array of setting information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static TArray<FEngineSettingInfo> ListSettings(const FString& CategoryId);

	/**
	 * Get detailed information about a specific setting.
	 *
	 * @param CategoryId - Category identifier
	 * @param Key - Setting key name
	 * @param OutInfo - Output structure with setting details
	 * @return True if setting was found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static bool GetSettingInfo(const FString& CategoryId, const FString& Key, FEngineSettingInfo& OutInfo);

	// =================================================================
	// Get/Set Individual Settings
	// =================================================================

	/**
	 * Get a single engine setting value.
	 *
	 * @param CategoryId - Category identifier
	 * @param Key - Setting key name
	 * @return Setting value as string (empty if not found)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FString GetSetting(const FString& CategoryId, const FString& Key);

	/**
	 * Set a single engine setting value.
	 *
	 * @param CategoryId - Category identifier
	 * @param Key - Setting key name
	 * @param Value - New value as string
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FEngineSettingResult SetSetting(const FString& CategoryId, const FString& Key, const FString& Value);

	// =================================================================
	// Console Variables (CVars)
	// =================================================================

	/**
	 * Get a console variable value.
	 *
	 * @param Name - Console variable name (e.g., "r.ReflectionMethod", "gc.TimeBetweenPurgingPendingKillObjects")
	 * @return Current value as string (empty if not found)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FString GetConsoleVariable(const FString& Name);

	/**
	 * Set a console variable value.
	 * Note: Some cvars require restart to take effect.
	 *
	 * @param Name - Console variable name
	 * @param Value - New value as string
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FEngineSettingResult SetConsoleVariable(const FString& Name, const FString& Value);

	/**
	 * Get detailed information about a console variable.
	 *
	 * @param Name - Console variable name
	 * @param OutInfo - Output structure with cvar details
	 * @return True if cvar was found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static bool GetConsoleVariableInfo(const FString& Name, FConsoleVariableInfo& OutInfo);

	/**
	 * Search for console variables by name or description.
	 *
	 * @param SearchTerm - Search string (matches against name and description)
	 * @param MaxResults - Maximum number of results to return (0 = unlimited)
	 * @return Array of matching console variables
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static TArray<FConsoleVariableInfo> SearchConsoleVariables(const FString& SearchTerm, int32 MaxResults = 50);

	/**
	 * List all console variables with a specific prefix.
	 *
	 * @param Prefix - CVar prefix (e.g., "r.", "gc.", "p.")
	 * @param MaxResults - Maximum number of results (0 = unlimited)
	 * @return Array of matching console variables
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static TArray<FConsoleVariableInfo> ListConsoleVariablesWithPrefix(const FString& Prefix, int32 MaxResults = 100);

	// =================================================================
	// Batch Operations
	// =================================================================

	/**
	 * Get all settings in a category as a JSON object.
	 *
	 * @param CategoryId - Category identifier
	 * @return JSON object string with all key-value pairs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FString GetCategorySettingsAsJson(const FString& CategoryId);

	/**
	 * Set multiple settings in a category from a JSON object.
	 *
	 * @param CategoryId - Category identifier
	 * @param SettingsJson - JSON object string with key-value pairs
	 * @return Operation result with success/failure lists
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FEngineSettingResult SetCategorySettingsFromJson(const FString& CategoryId, const FString& SettingsJson);

	/**
	 * Set multiple console variables from a JSON object.
	 *
	 * @param SettingsJson - JSON object with cvar name->value pairs
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FEngineSettingResult SetConsoleVariablesFromJson(const FString& SettingsJson);

	// =================================================================
	// Direct Engine INI Access
	// =================================================================

	/**
	 * List all sections in an engine config file.
	 *
	 * @param ConfigFile - Config file name (e.g., "Engine.ini", "Input.ini", "Game.ini")
	 * @param bIncludeBase - Include base engine configs (not just project overrides)
	 * @return Array of section names
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static TArray<FString> ListEngineSections(const FString& ConfigFile, bool bIncludeBase = false);

	/**
	 * Get a value from an engine config file.
	 *
	 * @param Section - INI section (e.g., "/Script/Engine.RendererSettings")
	 * @param Key - Key name within the section
	 * @param ConfigFile - Config file name (e.g., "Engine.ini")
	 * @return Value as string (empty if not found)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FString GetEngineIniValue(const FString& Section, const FString& Key, const FString& ConfigFile);

	/**
	 * Set a value in an engine config file (project override).
	 *
	 * @param Section - INI section
	 * @param Key - Key name within the section
	 * @param Value - Value to set
	 * @param ConfigFile - Config file name
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FEngineSettingResult SetEngineIniValue(const FString& Section, const FString& Key, const FString& Value, const FString& ConfigFile);

	/**
	 * Get an array of values from an engine config file.
	 *
	 * @param Section - INI section
	 * @param Key - Key name within the section
	 * @param ConfigFile - Config file name
	 * @return Array of values
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static TArray<FString> GetEngineIniArray(const FString& Section, const FString& Key, const FString& ConfigFile);

	/**
	 * Set an array of values in an engine config file.
	 *
	 * @param Section - INI section
	 * @param Key - Key name within the section
	 * @param Values - Array of values to set
	 * @param ConfigFile - Config file name
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FEngineSettingResult SetEngineIniArray(const FString& Section, const FString& Key, const TArray<FString>& Values, const FString& ConfigFile);

	// =================================================================
	// Scalability Settings
	// =================================================================

	/**
	 * Get current scalability settings as a JSON object.
	 * Returns quality levels for all scalability groups.
	 *
	 * @return JSON object with scalability levels
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FString GetScalabilitySettings();

	/**
	 * Set scalability quality level for a group.
	 *
	 * @param GroupName - Scalability group (e.g., "ViewDistance", "AntiAliasing", "Shadow", "PostProcess", "Texture", "Effects", "Foliage", "Shading")
	 * @param QualityLevel - Quality level (0=Low, 1=Medium, 2=High, 3=Epic, 4=Cinematic)
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FEngineSettingResult SetScalabilityLevel(const FString& GroupName, int32 QualityLevel);

	/**
	 * Apply overall quality level to all scalability groups.
	 *
	 * @param QualityLevel - Quality level (0=Low, 1=Medium, 2=High, 3=Epic, 4=Cinematic)
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static FEngineSettingResult SetOverallScalabilityLevel(int32 QualityLevel);

	// =================================================================
	// Persistence
	// =================================================================

	/**
	 * Force save all pending engine config changes to disk.
	 *
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static bool SaveAllEngineConfig();

	/**
	 * Save a specific engine config file.
	 *
	 * @param ConfigFile - Config file name (e.g., "Engine.ini")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|EngineSettings")
	static bool SaveEngineConfig(const FString& ConfigFile);

private:
	/** Get the settings object for a category (may return nullptr) */
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

	/** Get console variable flags as string */
	static FString GetCVarFlagsString(IConsoleVariable* CVar);

	/** Get console variable type as string */
	static FString GetCVarTypeString(IConsoleVariable* CVar);
};
