// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Per-process health heartbeat for MCP agents (issue #555).
 *
 * Epic's MCP endpoint is serviced entirely from the game thread and has no request timeout, so a
 * dead or wedged editor leaves clients hanging until their own timeout (observed: 300s), and even
 * the JSON-RPC "ping" hangs with it. This heartbeat is the out-of-band answer: a background thread
 * writes Project/Saved/VibeUE/Signals/editor-<pid>-health.json every ~5 seconds, while an FTSTicker
 * stamps the last game-thread tick time.
 *
 * How an agent reads it:
 *  - File missing, or `updatedUtc` older than ~15s  -> the editor process is gone (crashed/exited).
 *  - `gameThreadStallSeconds` above ~10             -> the process is alive but the game thread is
 *    wedged (modal dialog, crash handler, hang) — MCP calls WILL hang; relaunch instead of waiting.
 *  - Fresh file, small stall                        -> the editor is healthy; an MCP failure is
 *    something else (config, port, transport).
 *
 * Fields: signal ("health"), pid, updatedUtc, sessionStartUtc, gameThreadStallSeconds. Written to a
 * .tmp sibling and moved into place, so a watcher never observes a half-written file.
 */
class VIBEUE_API FVibeUEHealthSignal
{
public:
	/** Health file path for an arbitrary process id. Does not create anything. */
	static FString GetHealthPathForPid(uint32 ProcessId);

	/** Start the game-thread tick stamp and the background writer. Idempotent. */
	static void Start();

	/** Stop the writer, remove the ticker, and delete this process's health file. Safe to repeat. */
	static void Stop();
};
