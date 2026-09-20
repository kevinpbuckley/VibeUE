// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Result of a throwaway loopback bind on the MCP port. A bind from inside the process that should own
 * the port cannot tell whether WE hold the listening socket or another process does — either way the
 * bind fails with "address in use" — so `InUse` means only "some socket holds the port", never "we do".
 */
enum class EVibeUEMcpPortProbe : uint8
{
	Free,       // bind succeeded — nothing is listening on the port
	InUse,      // bind failed with address-in-use — a socket already holds the port
	ProbeError  // the probe could not run (no socket subsystem, socket creation failed, or bind failed for another reason)
};

/**
 * The single decision the MCP-status state machine can reach for a given (auto-start, module-listening,
 * probe, elapsed) tuple. Everything except the two logging concerns is a no-op; the logging actions each
 * map to exactly one Error line so the port fight leaves a loud, non-spammy trail.
 */
enum class EVibeUEMcpStatusAction : uint8
{
	Wait,             // auto-start requested, listener not up yet, still inside the startup grace — say nothing, keep waiting
	Silent,           // healthy, or none of this process's business — say nothing
	LogFreeError,     // grace expired, no local server, port FREE — this editor's MCP server failed to start / never started
	LogOccupiedError, // grace expired, no local server, port held by ANOTHER process — this editor lost the port fight
	LogProbeError,    // grace expired, no local server, probe could not run — cannot confirm a live listener
	LogContradiction  // auto-start NOT pending yet the module claims it is listening while the port probes FREE — a genuine lie
};

/**
 * MCP endpoint status for the readiness / health signals (issue B6).
 *
 * When a headless UnrealEditor-Cmd and the GUI editor start together they fight over MCP port 8000;
 * the loser prints one LogHttpListener "unable to bind" line and otherwise looks healthy while every
 * MCP call silently fails to connect. These helpers surface the port and whether this process's MCP
 * module reports a running server, plus a socket bind-probe cross-check.
 *
 * Startup race (the bug this class was reworked to fix): the readiness signal is published the moment
 * RegisterToolsets() ends, ~10 ms BEFORE Epic's MCP module binds its HTTP listener. A bind probe at
 * that instant always finds the port free even though MCP is perfectly healthy. So instead of logging
 * an error immediately, OnReadinessPublished() opens a bounded grace window (see StartupGraceSeconds):
 * it re-probes on a ticker, republishes the readiness signal the moment the listener reports running
 * (so `mcpListening` flips to true without waiting for a map-open), and only logs a single error if the
 * grace expires with no local server — classifying free-port vs another-process-holds-it vs probe-failure.
 */
class VIBEUE_API FVibeUEMcpStatus
{
public:
	/** Total time to wait for THIS process's MCP listener to bind after toolset registration. */
	static constexpr double StartupGraceSeconds = 15.0;

	/** Re-probe cadence (and first-fire delay) while inside the startup grace window. */
	static constexpr float StartupProbeIntervalSeconds = 1.0f;

	/**
	 * Read the MCP endpoint status. GAME-THREAD ONLY (touches the ModelContextProtocol module).
	 * @param OutPort      The active server port if a server is running, else the configured port
	 *                     (UE::ModelContextProtocol::GetServerPortNumber(), which honours
	 *                     -ModelContextProtocolPort=N and falls back to the settings CDO / 8000).
	 * @param OutListening True when this process's MCP module reports the HTTP server running.
	 */
	static void Query(uint32& OutPort, bool& OutListening);

	/**
	 * Throwaway loopback bind on 127.0.0.1:Port, classifying the port as Free / InUse / ProbeError.
	 * Reuse-addr is forced OFF so an existing listener makes the bind fail; the socket is closed
	 * immediately either way. Address-in-use (SE_EADDRINUSE) is distinguished from any other bind
	 * failure, which classifies as ProbeError. Socket work is thread-agnostic (any thread safe).
	 */
	static EVibeUEMcpPortProbe ProbeLoopbackPort(uint32 Port);

	/**
	 * Pure decision function — the whole state machine, with no side effects, so it is exhaustively
	 * unit-testable. Given whether auto-start was requested, whether this process's MCP module reports
	 * listening, the port-probe result, and how long we have been waiting, it returns the one action to
	 * take. The ticker and the logging below are thin wrappers over this.
	 */
	static EVibeUEMcpStatusAction DecideAction(bool bAutoStart, bool bModuleListening,
		EVibeUEMcpPortProbe Probe, double ElapsedSeconds, double GraceSeconds);

	/** Emit the single Error line for a Log* action. No-op for Wait / Silent. */
	static void LogActionResult(EVibeUEMcpStatusAction Action, uint32 Port);

	/**
	 * Called right after the readiness signal is (re)published. Evaluates MCP status and either stays
	 * silent, logs the genuine contradiction immediately, or (during startup, when the listener has not
	 * come up yet) opens a bounded grace window that republishes the readiness signal once the listener
	 * binds or logs one error if the grace expires. Idempotent: reaches a terminal conclusion once per
	 * process, so later republishes (e.g. on map-open) never re-log or restack the ticker.
	 * GAME-THREAD ONLY.
	 */
	static void OnReadinessPublished(uint32 Port, bool bModuleListening);

	/**
	 * Begin (or, if already waiting, leave running) the bounded startup-grace ticker. Registers a
	 * FTSTicker that re-probes every StartupProbeIntervalSeconds until the listener reports running or
	 * StartupGraceSeconds elapses. Normally driven by OnReadinessPublished; exposed for tests. Registering
	 * the ticker touches no engine module — only the per-tick callback does. GAME-THREAD ONLY.
	 */
	static void ScheduleStartupGraceTicker(uint32 Port);

	/** Cancel any pending startup-grace ticker. Safe to call when none is scheduled. GAME-THREAD ONLY. */
	static void CancelStartupGraceTicker();

	/** True while a startup-grace ticker is registered and has not yet reached a terminal conclusion. */
	static bool IsStartupGraceTickerPending();
};
