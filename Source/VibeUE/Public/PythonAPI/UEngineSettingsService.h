// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UEngineSettingsService.generated.h"

/**
 * Result of an engine settings operation.
 * Python access: result = unreal.EngineSettingsService.set_engine_ini_value(section, key, value, config_file)
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
 * Play-In-Editor multiplayer settings snapshot.
 * Python access: info = unreal.EngineSettingsService.get_pie_settings()
 *
 * Properties:
 * - success (bool): Whether the settings could be read
 * - error_message (str): Error message if failed (empty if success)
 * - net_mode (str): PIE net mode: "Standalone" | "ListenServer" | "Client" (maps EPlayNetMode).
 *                   "Client" is dedicated-server PIE — the editor's "Play As Client", where a
 *                   windowless dedicated server is spawned behind the scenes.
 * - num_clients (int): Number of client windows to open (PlayNumberOfClients)
 * - run_under_one_process (bool): Spawn all player windows in a single UE process
 * - launch_separate_server (bool): The "Launch Separate Server" toggle. UE 5.8's
 *                   ULevelEditorPlaySettings has no bDedicatedServer field; dedicated-server PIE is
 *                   NetMode="Client". This is the closest standing flag and is reported for context.
 * - last_play_mode (str): Last executed play-mode type (e.g. "PlayMode_InViewPort"), or "" if unknown
 */
USTRUCT(BlueprintType)
struct FPIESettingsInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString NetMode;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	int32 NumClients = 0;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	bool bRunUnderOneProcess = false;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	bool bLaunchSeparateServer = false;

	UPROPERTY(BlueprintReadWrite, Category = "EngineSettings")
	FString LastPlayMode;
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
 *   # Inspect engine config sections and values
 *   sections = unreal.EngineSettingsService.list_engine_sections("Engine.ini")
 *   value = unreal.EngineSettingsService.get_engine_ini_value(
 *       "/Script/Engine.Engine", "GameEngine", "Engine.ini")
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
class VIBEUE_API UEngineSettingsService : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	// =================================================================
	// Console Variables (CVars)
	// =================================================================






	// =================================================================
	// Batch Operations
	// =================================================================


	// =================================================================
	// Direct Engine INI Access
	// =================================================================






	// =================================================================
	// Scalability Settings
	// =================================================================




	// =================================================================
	// Play-In-Editor (PIE) net-mode settings
	// =================================================================

	/**
	 * Read the editor's Play-In-Editor multiplayer settings (ULevelEditorPlaySettings).
	 *
	 * These are per-USER config (Saved/Config/.../EditorPerProjectUserSettings.ini), not project
	 * config, so they follow the machine, not the repo. PlayNetMode is otherwise unreadable from
	 * Python (the raw EPlayNetMode enum cannot be pythonized).
	 *
	 * @return Snapshot with net_mode, num_clients, run_under_one_process, launch_separate_server,
	 *         last_play_mode. success=false with error_message if the settings object is unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category ="VibeUE|EngineSettings")
	static FPIESettingsInfo GetPIESettings();

	/**
	 * Write the editor's Play-In-Editor net-mode settings and persist them so they survive an
	 * editor restart (SaveConfig to EditorPerProjectUserSettings.ini). This makes dedicated-server
	 * PIE gates scriptable without hand-editing the ini and relaunching.
	 *
	 * @param NetMode - "Standalone" | "ListenServer" | "Client" (case-insensitive; "DedicatedServer"
	 *                  is accepted as an alias for "Client"). Unknown values are refused (Warning).
	 * @param NumClients - Number of client windows, 1-10. Out-of-range values are refused (Warning).
	 * @param bRunUnderOneProcess - Spawn all player windows in a single UE process.
	 * @return True only if every written value reads back as requested; false (with a Warning) on a
	 *         mismatch or an invalid argument. Verify with get_pie_settings.
	 */
	UFUNCTION(BlueprintCallable, Category ="VibeUE|EngineSettings")
	static bool SetPIESettings(const FString& NetMode, int32 NumClients, bool bRunUnderOneProcess);

	// =================================================================
	// Persistence
	// =================================================================



private:

	/** Get console variable flags as string */
	static FString GetCVarFlagsString(IConsoleVariable* CVar);

	/** Get console variable type as string */
	static FString GetCVarTypeString(IConsoleVariable* CVar);
};
