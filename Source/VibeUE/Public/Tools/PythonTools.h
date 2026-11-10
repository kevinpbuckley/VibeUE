// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PythonTools.generated.h"

/**
 * Editor-safety helpers for scripts run through the engine's execute_python.
 *
 * Registered as AICallable tools via ToolsetRegistry. Python execution and API discovery
 * themselves are engine-native in 5.9 (EditorToolset.PythonScriptingTools).
 */
UCLASS(meta=(ToolCategory="Python"))
class VIBEUE_API UPythonTools : public UObject
{
	GENERATED_BODY()

public:

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

};
