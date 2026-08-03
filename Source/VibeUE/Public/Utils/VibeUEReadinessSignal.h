// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Per-process readiness signal for MCP agents (PR #527).
 *
 * VibeUE writes Project/Saved/VibeUE/Signals/editor-<pid>-true.json when FModule::RegisterToolsets()
 * reaches its end, and deletes it on module shutdown / editor pre-exit. Agents that launch the editor
 * via BuildAndLaunchGame.ps1|sh read `Editor-PID=<pid>` from the script output and watch for that one
 * file instead of polling MCP.
 *
 * The signal means only "toolset registration finished for THIS process". Python, World and level
 * readiness remain separate checks.
 *
 * The file carries real JSON (pid, createdUtc, sessionStartUtc, pluginVersion) so a watcher can tell
 * a fresh signal from one left behind by a crashed editor whose PID the OS later reused: compare
 * `sessionStartUtc` against the time you launched the process. It is written to a .tmp sibling and
 * moved into place, so a watcher never observes a half-written file.
 */
class VIBEUE_API FVibeUEReadinessSignal
{
public:
	/** Signal file path for an arbitrary process id. Does not create anything. */
	static FString GetSignalPathForPid(uint32 ProcessId);

	/** Signal file path for the running editor process. */
	static FString GetSignalPath();

	/** Approximate UTC start time of this editor process, derived from GStartTime. */
	static FDateTime GetSessionStartUtc();

	/** Build the signal payload. Pure — split out so automation tests can verify it headlessly. */
	static FString BuildSignalJson(uint32 ProcessId, const FDateTime& SessionStartUtc, const FDateTime& CreatedUtc);

	/** Write the signal for this process. Returns false (and logs) if the directory or file write fails. */
	static bool Publish();

	/** Delete this process's signal. Safe to call when it does not exist. */
	static void Remove();
};
