"""
Unified Blueprint node management tools for the VibeUE MCP server.

This module provides a unified interface for performing various operations on Blueprint nodes,
such as adding, removing, and connecting nodes, as well as managing Blueprint functions and variables.
"""

from __future__ import annotations

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

logger = logging.getLogger("UnrealMCP")


def _merge(target: Dict[str, Any], values: Dict[str, Any]) -> None:
    """Merge non-``None`` values into ``target`` in-place."""
    for key, value in values.items():
        if value is not None:
            target[key] = value


def register_blueprint_node_tools(mcp: FastMCP) -> None:
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

    @mcp.tool()
    def manage_blueprint_node(
        ctx: Context,
        blueprint_name: str,
        action: str,
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
        """
        Perform Blueprint node operations via a single unified tool.
        
        ⚠️ **IMPORTANT**: Before creating nodes (action='create'), ALWAYS use get_available_blueprint_nodes()
        first to discover exact node type names. This prevents errors and ensures reliable node creation.
        
        🔄 **RECOMMENDED WORKFLOW**:
        1. Call get_available_blueprint_nodes() to discover available node types
        2. Use exact node names from discovery results in node_type parameter
        3. Create nodes with precise type names for guaranteed success
        
        **Actions**:
        - create: Create new nodes (requires exact node_type from get_available_blueprint_nodes)
        - connect: Connect node pins
        - delete: Remove nodes
        - move: Reposition nodes
        - configure: Set node properties
        
        **Examples**:
        ```python
        # Step 1: Discover available nodes
        nodes = get_available_blueprint_nodes("BP_Player", category="Flow Control")
        
        # Step 2: Use exact node name from discovery
        manage_blueprint_node(
            blueprint_name="BP_Player",
            action="create", 
            node_type="Branch",  # Exact name from discovery
            position=[200, 100]
        )
        ```
        
        🎯 **Node Type Discovery**: Use get_available_blueprint_nodes() with search terms like:
        - "Flow Control" category for branches, loops
        - "Math" category for calculations  
        - "Variables" category for get/set operations
        - "Functions" category for custom functions
        """

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
                "node_params": node_params,
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

    @mcp.tool()
    def get_available_blueprint_nodes(
        blueprint_name: str,
        category: str = "",
        graph_scope: str = "event",
        include_functions: bool = True,
        include_variables: bool = True,
        include_events: bool = True,
        max_results: int = 100,
    ) -> Dict[str, Any]:
        """
        🔍 **ESSENTIAL NODE DISCOVERY TOOL**: Get all available Blueprint node types using Unreal's reflection system.
        
        ⚠️ **USE THIS FIRST**: Always call this before manage_blueprint_node(action='create') to discover
        exact node type names. This prevents "unknown node type" errors and ensures reliable node creation.
        
        🎯 **PRIMARY USE CASES**:
        - **Before Creating Nodes**: Find exact node type names (e.g., "Branch", "Print String", "Cast To Object")
        - **Node Type Resolution**: When you know what you want to do but need the precise node name
        - **Category Exploration**: Browse available nodes by category (Flow Control, Math, Variables, etc.)
        - **Function-Specific Nodes**: Discover nodes available in different graph contexts
        
        🔄 **TYPICAL AI WORKFLOW**:
        ```python
        # 1. User says: "Create a branch node"
        nodes = get_available_blueprint_nodes("BP_Player", category="Flow Control")
        # Result: Find "Branch" in the node list
        
        # 2. Use exact name for creation
        manage_blueprint_node(blueprint_name="BP_Player", action="create", 
                            node_type="Branch", position=[200, 100])
        ```
        
        📊 **Search Strategies**:
        - **By Intent**: category="Flow Control" for conditionals, loops
        - **By Function**: category="Math" for calculations
        - **By Action**: category="Variables" for data operations  
        - **By Context**: graph_scope="function" vs "event" for appropriate nodes
        
        Args:
            blueprint_name: Name of the target Blueprint to get nodes for
            category: Filter by node category (Flow Control, Math, Variables, Functions, etc.)
            graph_scope: Context for node discovery ("event" for Event Graph, "function" for Function Graph)
            include_functions: Whether to include function call nodes
            include_variables: Whether to include variable get/set nodes
            include_events: Whether to include event nodes  
            max_results: Maximum number of nodes to return (default: 100)
            
        Returns:
            Dict containing:
            - success: boolean indicating if discovery completed
            - categories: dict of node categories with arrays of available nodes
            - total_nodes: total number of nodes found
            - error: string (only if success=false)
            
        � **Node Metadata Structure**:
        Each discovered node includes:
        - name: Exact node type name to use in manage_blueprint_node
        - category: Node category for organization
        - description: What the node does
        - keywords: Search terms for semantic discovery
        - type: Node classification (node, function, variable, etc.)
        
        💡 **Smart Usage Examples**:
        ```python
        # Find print/debug nodes
        get_available_blueprint_nodes("BP_Player", category="Development")
        
        # Find mathematical operations
        get_available_blueprint_nodes("BP_Player", category="Math")
        
        # Find conditional logic nodes
        get_available_blueprint_nodes("BP_Player", category="Flow Control")
        
        # Find variable operations
        get_available_blueprint_nodes("BP_Player", category="Variables")
        ```
        
        🎯 **AI Integration**: This tool enables data-driven Blueprint development by providing
        exact node names that work reliably, eliminating guesswork and trial-and-error.
        """
        
        payload = {
            "blueprint_name": blueprint_name,
            "category": category,
            "graph_scope": graph_scope,
            "include_functions": include_functions,
            "include_variables": include_variables,
            "include_events": include_events,
            "max_results": max_results,
        }

        return _dispatch("get_available_blueprint_nodes", payload)

    @mcp.tool()
    def manage_blueprint_function(
        ctx: Context,
        blueprint_name: str,
        action: str,
        function_name: Optional[str] = None,
        param_name: Optional[str] = None,
        direction: Optional[str] = None,
        type: Optional[str] = None,
        new_type: Optional[str] = None,
        new_name: Optional[str] = None,
        properties: Optional[Dict[str, Any]] = None,
        extra: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Unified tool for Blueprint function graph operations."""

        payload: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "action": action,
        }

        _merge(
            payload,
            {
                "function_name": function_name,
                "param_name": param_name,
                "direction": direction,
                "type": type,
                "new_type": new_type,
                "new_name": new_name,
                "properties": properties,
            },
        )

        if extra:
            _merge(payload, extra)

        return _dispatch("manage_blueprint_function", payload)

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
