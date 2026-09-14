// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * MCP endpoint status for the readiness / health signals (issue B6).
 *
 * When a headless UnrealEditor-Cmd and the GUI editor start together they fight over MCP port 8000;
 * the loser prints one LogHttpListener "unable to bind" line and otherwise looks healthy while every
 * MCP call silently fails to connect. These helpers surface the port and whether this process's MCP
 * module reports a running server, plus a socket bind-probe cross-check that leaves a loud Error line
 * when the two disagree.
 *
 * Limitation (documented on purpose): a TCP bind probe issued from inside the process that should own
 * the port cannot tell whether WE hold the listening socket or another process does — a bind fails
 * with "address in use" in both cases. So `bListening` is the MCP module's own claim
 * (FModelContextProtocolServer::IsServerRunning), and the probe only catches the case where the
 * module thinks it is serving but the port is actually FREE (the listener never bound). See the
 * `vibeue` skill for how an agent should read the published fields.
 */
class VIBEUE_API FVibeUEMcpStatus
{
public:
	/**
	 * Read the MCP endpoint status. GAME-THREAD ONLY (touches the ModelContextProtocol module).
	 * @param OutPort      The active server port if a server is running, else the configured port
	 *                     (UE::ModelContextProtocol::GetServerPortNumber(), which honours
	 *                     -ModelContextProtocolPort=N and falls back to the settings CDO / 8000).
	 * @param OutListening True when this process's MCP module reports the HTTP server running.
	 */
	static void Query(uint32& OutPort, bool& OutListening);

	/**
	 * Bind-probe cross-check. When this process intends to serve MCP but a bind on 127.0.0.1:Port
	 * finds the port free (or the probe cannot run), log at Error naming the port so the port-8000
	 * fight leaves a loud line. No-op when this process is not expected to serve MCP. GAME-THREAD /
	 * any thread safe (sockets are thread-agnostic); intended for the readiness publish, not the 5s
	 * heartbeat, so it does not spam.
	 */
	static void LogPortContradictionIfAny(uint32 Port, bool bModuleListening);
};
