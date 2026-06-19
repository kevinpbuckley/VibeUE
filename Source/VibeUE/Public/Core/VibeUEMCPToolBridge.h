// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Bridges VibeUE's dynamic FToolRegistry tools (registered via REGISTER_VIBEUE_TOOL) onto
 * UE 5.8's native ModelContextProtocol server, so they appear as ordinary MCP tools on Epic's
 * endpoint. VibeUE no longer runs its own MCP server — Epic's is the single endpoint.
 */
namespace VibeUEMCPToolBridge
{
	/** Wrap every non-internal VibeUE tool as an IModelContextProtocolTool and AddTool() it to Epic's MCP module. */
	VIBEUE_API void RegisterAll();

	/** Remove the tools registered by RegisterAll(). */
	VIBEUE_API void UnregisterAll();
}
