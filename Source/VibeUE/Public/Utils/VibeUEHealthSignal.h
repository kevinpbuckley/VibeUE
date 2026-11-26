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
 *  - `slowTask` or `modal` true                     -> the editor is busy, not wedged (issue #681): the
 *    MCP server refuses tool calls ("Not run: the Unreal Editor is running a slow task" / "is showing a
 *    modal dialog") and the stall figure grows while it lasts. Watch the file and call again once both
 *    are false; `slowTaskText` / `modalTitle` say what it is. Takes precedence over the stall rule.
 *  - Fresh file, small stall                        -> the editor is healthy; an MCP failure is
 *    something else (config, port, transport).
 *
 *  - `shaderJobs` / `assetCompilations` above 0      -> ready, but still compiling shaders or assets in the
 *    background (issue #697): captures and frame timings are not representative yet. -1 = not sampled yet.
 *
 * Fields: signal ("health"), pid, updatedUtc, sessionStartUtc, gameThreadStallSeconds, mcpPort,
 * mcpListening, shaderJobs, assetCompilations, slowTask, slowTaskText, slowTaskSeconds, modal, modalTitle,
 * modalSeconds. Written to a .tmp sibling and moved into place, so a watcher never observes a half-written file.
 *
 * Slow-task and modal state is set from FSlowTask::OnProgress and Slate's modal-loop tick, the game-thread
 * callbacks that still run while the editor's normal loop is blocked, and cleared by the core ticker once the
 * loop runs again. A slow task that reports no progress, or a native OS message box (not a Slate modal), is not
 * seen: then only the stall figure grows.
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

	/**
	 * Whether the editor is inside a slow task right now (the same test the MCP server uses to refuse tool calls), and
	 * its text (the outermost non-empty slow-task message) when OutText is given. GAME-THREAD ONLY. (#700)
	 */
	static bool IsInSlowTask(FString* OutText = nullptr);

	/** Whether a Slate modal window is open right now, and its title when OutTitle is given. GAME-THREAD ONLY. (#700) */
	static bool IsModalWindowOpen(FString* OutTitle = nullptr);
};
