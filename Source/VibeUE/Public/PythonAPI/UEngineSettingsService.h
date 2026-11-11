// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UEngineSettingsService.generated.h"



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
 * Engine Settings Service - two Python-only PIE helpers. This service has NO AI tools.
 *
 * Engine settings are engine-native on the 5-9 branch, so nothing here is a tool any more. Cvars,
 * console commands and scalability are `ConsoleToolset`; everything in Project Settings or Editor
 * Preferences -- ULevelEditorPlaySettings included, at Editor / LevelEditor / PlayIn -- is
 * `ConfigSettingsToolset`; a raw ini key with no registered section is `FileSystemToolset`. Not
 * ObjectToolset: a UDeveloperSettings object is a native CDO, which SetProperties refuses. See the
 * `engine-settings` skill.
 *
 * What is left is two DEMOTED helpers, not tools. GetPIESettings / SetPIESettings are no longer
 * MCP tools and will not appear in list_toolsets, but are still callable as
 * `unreal.EngineSettingsService.get_pie_settings()` and
 * `.set_pie_settings(net_mode, num_clients, run_under_one_process)` from inside execute_python.
 * They exist for exactly one caller: Content/Python/vibeue.py's GateSession, which reads, applies
 * and restores the PIE net mode from inside execute_python, where Epic's toolsets are unreachable
 * (none of those three carry PythonCallable). For ordinary PIE configuration use
 * ConfigSettingsToolset.
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
