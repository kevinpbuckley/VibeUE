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

	/**
	 * Stop the writer (joined), remove the ticker, and delete this process's health file. Safe to repeat, and safe
	 * after RequestStopFromAnyThread. Off the game thread it does RequestStopFromAnyThread instead (no join).
	 */
	static void Stop();

	/**
	 * Forced-exit variant of Stop() for ANY thread (the Windows console-control thread on Ctrl-C/close/logoff,
	 * or a crashing worker, possibly the writer itself, inside RequestExit(true)): sets the stop flag, wakes the
	 * writer WITHOUT joining it, and deletes the health file. A heartbeat in flight does not leave the file behind:
	 * the writer re-checks the flag under the shared lock, and a write that was already past that check deletes
	 * the files again after its move. Only a hard exit (TerminateProcess) landing between that move and the
	 * writer's own delete can still leave one. Owns nothing: the thread, event and ticker stay with the game
	 * thread, so a later Stop() (OnPreExit after a Ctrl-C) still joins and frees them. Waits about 0.2 s at most
	 * (wall clock) for the writer's lock.
	 */
	static void RequestStopFromAnyThread();

	/** True while the game thread owns a writer thread that Stop() has not joined yet (even if it is already stopping). GAME-THREAD ONLY. */
	static bool IsWriterThreadOwned();
};
