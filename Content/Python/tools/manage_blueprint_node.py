"""
Unified Blueprint node management tools for the VibeUE MCP server.

This module provides a unified interface for performing various operations on Blueprint nodes,
such as adding, removing, and connecting nodes, as well as managing Blueprint functions and variables.
"""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

logger = logging.getLogger("UnrealMCP")


def _merge(target: Dict[str, Any], values: Dict[str, Any]) -> None:
    """Merge non-``None`` values into ``target`` in-place."""
    for key, value in values.items():
        if value is not None:
            target[key] = value


def register_node_tools(mcp: FastMCP) -> None:
    """Register unified Blueprint node MCP tools."""

    def _dispatch(command: str, payload: Dict[str, Any]) -> Dict[str, Any]:
        from vibe_ue_server import get_unreal_connection

        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        logger.info("Dispatching %s with payload: %s", command, payload)
        response = unreal.send_command(command, payload)
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "message": "No response from Unreal Engine"}

        logger.debug("%s response: %s", command, response)
        return response

    @mcp.tool(description="Blueprint node operations: discover, create, connect, delete, move, configure. Use spawner_key from discover for exact node creation. Actions: discover, create, connect_pins, disconnect_pins, delete, move, list, describe, configure, get_details, split, recombine, refresh_node, refresh_nodes. Use get_help(topic='node-tools') for examples.")
    def manage_blueprint_node(
        ctx: Context,
        action: str,
        blueprint_name: str = "",
        graph_scope: str = "event",
        function_name: Optional[str] = None,
        node_id: Optional[str] = None,
        node_identifier: Optional[str] = None,
        node_type: Optional[str] = None,
        node_params: Optional[Dict[str, Any]] = None,
        node_config: Optional[Dict[str, Any]] = None,
        position: Optional[List[float]] = None,
        node_position: Optional[List[float]] = None,
        source_node_id: Optional[str] = None,
        source_pin: Optional[str] = None,
        target_node_id: Optional[str] = None,
        target_pin: Optional[str] = None,
        disconnect_pins: Optional[bool] = None,
        property_name: Optional[str] = None,
        property_value: Optional[Any] = None,
        include_functions: Optional[bool] = None,
        include_macros: Optional[bool] = None,
        include_timeline: Optional[bool] = None,
        extra: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Route to Blueprint node action handlers."""

        # Handle discovery action in Python (doesn't need C++ dispatch)
        if action and action.lower() in ["discover", "get_available_nodes", "search_nodes"]:
            # Extract discovery parameters from extra dict
            discover_params = extra or {}
            return _get_available_blueprint_nodes_internal(
                blueprint_name=blueprint_name,
                category=discover_params.get("category", ""),
                search_term=discover_params.get("search_term", ""),
                graph_scope=graph_scope or "event",
                include_functions=discover_params.get("include_functions", True),
                include_variables=discover_params.get("include_variables", True),
                include_events=discover_params.get("include_events", True),
                max_results=discover_params.get("max_results", 100),
                return_descriptors=discover_params.get("return_descriptors", True),
            )

        normalized_node_params = node_params

        payload: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "action": action,
            "graph_scope": graph_scope,
        }

        resolved_node_type = node_type or node_identifier
        _merge(
            payload,
            {
                "function_name": function_name,
                "node_id": node_id,
                "node_type": resolved_node_type,
                "node_identifier": resolved_node_type,
                "node_params": normalized_node_params,
                "node_config": node_config,
                "position": position,
                "node_position": node_position,
                "source_node_id": source_node_id,
                "source_pin": source_pin,
                "target_node_id": target_node_id,
                "target_pin": target_pin,
                "disconnect_pins": disconnect_pins,
                "property_name": property_name,
                "property_value": property_value,
                "include_functions": include_functions,
                "include_macros": include_macros,
                "include_timeline": include_timeline,
            },
        )

        if extra:
            _merge(payload, extra)

        # Debug logging to understand parameter passing
        logger.info(f"manage_blueprint_node final payload: {payload}")

        # Avoid duplicating identical node configuration dictionaries
        if payload.get("node_config") is payload.get("node_params"):
            payload.pop("node_config", None)

        return _dispatch("manage_blueprint_node", payload)

    def _get_available_blueprint_nodes_internal(
        blueprint_name: str,
        category: str = "",
        search_term: str = "",
        graph_scope: str = "event",
        include_functions: bool = True,
        include_variables: bool = True,
        include_events: bool = True,
        max_results: int = 100,
        return_descriptors: bool = True,
    ) -> Dict[str, Any]:
        """
        Internal helper for node discovery. Use manage_blueprint_node(action="discover") instead.
        
        Returns node descriptors with spawner_key for exact creation.
        """
        
        payload = {
            "blueprint_name": blueprint_name,
            "category": category,
            "search_term": search_term,
            "graph_scope": graph_scope,
            "include_functions": include_functions,
            "include_variables": include_variables,
            "include_events": include_events,
            "max_results": max_results,
            "return_descriptors": return_descriptors,
        }

        return _dispatch("get_available_blueprint_nodes", payload)

    logger.info("Unified Blueprint node tools registered")


def manage_blueprint_node_test(
    blueprint_name: str,
    action: str,
    **params: Any,
) -> Dict[str, Any]:
    """Lightweight helper for exercising the MCP contract in tests."""

    result = {
        "success": True,
        "blueprint_name": blueprint_name,
        "action": action,
        "params": params,
        "message": "Test helper executed successfully.",
    }
    logger.debug("manage_blueprint_node_test payload: %s", result)
    return result
