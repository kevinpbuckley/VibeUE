// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PythonTools.generated.h"

// Forward declarations - in VibeUE namespace
namespace VibeUE
{
	class FPythonExecutionService;
	class FPythonSchemaService;
	struct FPythonModuleInfo;
	struct FPythonClassInfo;
	struct FPythonFunctionInfo;
	struct FPythonExecutionResult;
}

/**
 * MCP tools for Python code execution and discovery in Unreal Engine
 *
 * These tools are automatically exposed to MCP clients via the ToolRegistry.
 * They allow AI assistants to execute Python code and discover Python APIs.
 */
UCLASS(meta=(ToolCategory="Python"))
class VIBEUE_API UPythonTools : public UObject
{
	GENERATED_BODY()

public:
	UPythonTools();

	/**
	 * Execute Python code in the Unreal Engine Python environment
	 *
	 * @param Code - Python code to execute
	 * @param bAutoSave - When true (default) every dirty content and world package is saved headlessly
	 *                    before the script runs; pass false to run the script without that sweep.
	 *                    The result JSON always reports the OUTCOME: auto_save (true only when the
	 *                    sweep really ran), auto_save_note and the saved_packages list.
	 *                    It also always carries dirtied_packages / cleaned_packages: the packages the
	 *                    SCRIPT left dirty / clean, from snapshots taken after the sweep and right
	 *                    after the script (on an error reply too).
	 * @param bFreshNamespace - Run in a namespace of its own, dropped when the script ends, instead of the
	 *                    globals every call shares (a global left there can keep a level or asset alive).
	 * @return Execution result including output, errors, and success status
	 */
	UFUNCTION(BlueprintCallable, Category="VibeUE|Python", meta=(
		ToolName="execute_python_code",
		ToolDescription="Execute Python code in Unreal Engine. Returns stdout, stderr, and execution status.",
		ParamDescription_Code="Python code to execute",
		ParamDescription_bAutoSave="Save all dirty content and world packages before running (default true). Pass false to skip the pre-run save sweep.",
		ParamDescription_bFreshNamespace="Run in a new namespace dropped when the script ends, instead of the globals every call shares (default false)."
	))
	static FString ExecutePythonCode(const FString& Code, bool bAutoSave = true, bool bFreshNamespace = false);

	/**
	 * Whether Code can only run as a coroutine: it has a top-level await, async for or async with. Such a
	 * script goes to StartAwaitRun; ExecutePythonCode would report a SyntaxError for it. A script that merely
	 * defines async functions, or that does not compile, is false.
	 */
	static bool NeedsAwaitRun(const FString& Code);

	/**
	 * Start a script that NeedsAwaitRun. The editor keeps ticking at each await, and OnDone gets the same
	 * JSON reply ExecutePythonCode returns, on the game thread, when the script has ended (immediately when
	 * it is refused). Game thread only.
	 *
	 * @param CancelKey - What CancelAwaitRuns finds this run by
	 */
	static void StartAwaitRun(const FString& Code, bool bAutoSave, bool bFreshNamespace, const FString& CancelKey, TFunction<void(const FString&)> OnDone);

	/** Raise CancelledError at the current await of every run started with CancelKey. Game thread only. */
	static void CancelAwaitRuns(const FString& CancelKey);

	/** Answer every run as cancelled and stop advancing them, without calling into Python: for editor exit. */
	static void AbandonAwaitRuns();

	/** How many scripts are paused at an await right now. */
	static int32 GetAwaitRunCount();

	// Helper functions for converting service results to JSON - made public for use in registration
	static FString ConvertExecutionResultToJson(const VibeUE::FPythonExecutionResult& Result);

	/**
	 * Clean up all static service instances.
	 * Must be called before Python shuts down to avoid access violations.
	 */
	static void Shutdown();

	/**
	 * Map worlds currently resident besides the open level, as full object paths.
	 *
	 * A map opened as an ASSET (unreal.load_asset("/Game/Maps/Foo"), EditorAssetLibrary.load_asset,
	 * find_object, ...) stays loaded afterwards. The engine checks on every level load that no other
	 * map package is still alive, and that check is FATAL, not a warning - so the crash lands on
	 * whoever loads a level next, naming neither the script nor the tool that left it behind.
	 * execute_python_code reports this after every run so the warning arrives with its cause.
	 *
	 * Excludes the open editor world, PIE worlds, and the transient preview worlds the editor keeps
	 * alive for Blueprint/material/thumbnail previews. Empty when GEditor is unavailable.
	 */
	static TArray<FString> GetResidentMapWorlds();

	/**
	 * Map worlds currently resident besides the open level, as full object paths, readable from inside a
	 * script. Same rule as the resident_maps reply field EXCEPT that worlds which are streaming sublevels
	 * of the open level are left out here (a level change unloads them with it, so they are not
	 * stragglers). The reply field keeps upstream's broader rule.
	 *
	 * Non-empty means the NEXT level load fails the engine's stale-world check and FATALS the editor.
	 * vibeue.safe_load_level() checks this before it loads anything.
	 *
	 * @return Full object paths of the resident map worlds (e.g. "/Game/Maps/Foo.Foo"); empty when none.
	 *
	 * Python usage:
	 *   stranded = unreal.PythonTools.get_resident_maps()
	 *   if stranded:
	 *       print("restart the editor before changing level:", list(stranded))
	 */
	UFUNCTION(BlueprintCallable, Category="VibeUE|Python", meta=(AICallable))
	static TArray<FString> GetResidentMaps();

	/**
	 * Names of the packages that are dirty right now, sorted: the content packages, plus the world
	 * packages (maps and their external actor packages) when bIncludeMaps is true. Same enumeration
	 * as the execute_python_code auto-save sweep (FEditorFileUtils::GetDirtyContentPackages /
	 * GetDirtyWorldPackages), so it is exactly the set an auto_save=true call would write.
	 *
	 * Two engine side effects to expect with bIncludeMaps: the world enumeration MARKS a dirty map's
	 * MapBuildData package dirty when that package has no saved name yet (FileHelpers.cpp
	 * EnumerateDirtyWorldPackages), and a never-saved map shows up under its /Temp/Untitled_N name. Both
	 * can therefore appear in execute_python_code's dirtied_packages.
	 *
	 * @param bIncludeMaps - Also list dirty world packages (default true)
	 * @return Sorted package names (e.g. "/Game/UI/W_Prompt"); empty when nothing is dirty or GEditor is unavailable.
	 *
	 * Python usage:
	 *   before = set(unreal.PythonTools.get_dirty_packages())
	 *   # ... edit ...
	 *   dirtied = set(unreal.PythonTools.get_dirty_packages()) - before
	 */
	UFUNCTION(BlueprintCallable, Category="VibeUE|Python", meta=(AICallable))
	static TArray<FString> GetDirtyPackages(bool bIncludeMaps = true);

	/**
	 * Diff two dirty-package snapshots. OutDirtied = in After but not Before; OutCleaned = in Before but
	 * not After. Both come back sorted. Public so the reply fields can be tested without a script that
	 * has to clean a package.
	 */
	static void ComputeDirtyPackageDelta(const TArray<FString>& Before, const TArray<FString>& After,
		TArray<FString>& OutDirtied, TArray<FString>& OutCleaned);

private:
	static TSharedPtr<VibeUE::FPythonExecutionService> GetExecutionService();
	static TSharedPtr<VibeUE::FPythonSchemaService> GetSchemaService();
};
