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
        🛠️ **MULTI-ACTION BLUEPRINT NODE MANAGER**: Universal tool for all Blueprint node operations.
        
        ## Supported Actions:
        - **create**: Create new nodes (requires node_type from get_available_blueprint_nodes())
        - **connect**: Connect pins between nodes (requires source/target node_id and pin names)
        - **disconnect**: Disconnect pins (requires source/target node_id and pin names) 
        - **delete**: Remove nodes (requires node_id)
        - **move**: Reposition nodes (requires node_id and position)
        - **list**: List all nodes in graph (returns node inventory)
        - **get_details**: Get detailed node information (requires node_id)
        - **configure**: Set node properties (requires node_id, property_name, property_value)
        
        ## Context Requirements:
        - **graph_scope**: "event" (Event Graph) or "function" (Function Graph) 
        - **function_name**: Required when graph_scope="function" - specify target function name
        - **blueprint_name**: Always required - target Blueprint name
        
        ## Multi-Action Usage Patterns:
        
        ### Pattern 1: Node Creation Workflow
        ```python
        # 1. Discover → 2. Create → 3. Position → 4. Configure
        nodes = get_available_blueprint_nodes("BP_Player", category="Flow Control")
        result = manage_blueprint_node("BP_Player", action="create", node_type="Branch")
        node_id = result["node_id"]
        manage_blueprint_node("BP_Player", action="move", node_id=node_id, position=[200, 100])
        ```
        
        ### Pattern 2: Node Connection Workflow
        ```python
        # List → Identify → Connect → Verify
        nodes = manage_blueprint_node("BP_Player", action="list")
        manage_blueprint_node("BP_Player", action="connect", 
                            source_node_id="EventBeginPlay_1", source_pin="exec",
                            target_node_id="Branch_2", target_pin="exec")
        ```
        
        ### Pattern 3: Function Graph Operations
        ```python
        # Function context requires function_name
        manage_blueprint_node("BP_Player", action="create", graph_scope="function",
                            function_name="CalculateHealth", node_type="Add", position=[100, 50])
        ```
        
        ## Action-Specific Parameters:
        
        **create**: node_type (required), position (optional), node_params (optional)
        **connect**: source_node_id, source_pin, target_node_id, target_pin
        **disconnect**: source_node_id, source_pin, target_node_id, target_pin  
        **delete**: node_id
        **move**: node_id, position
        **list**: (no additional parameters)
        **get_details**: node_id
        **configure**: node_id, property_name, property_value
        
        ## Pin Connection Guide:
        Common pin names for connections:
        - **Execution**: "exec" (out) → "exec" (in)
        - **Boolean**: "Return Value" → "Condition"  
        - **Variables**: "Return Value" → parameter names
        - **Function Calls**: "Return Value" → input parameter names
        
        ⚠️ **CRITICAL**: Always use get_available_blueprint_nodes() before action="create" to get exact node_type names.
        
        🔧 **Advanced Multi-Action Examples**:
        ```python
        # Create variable getter, position it, connect to branch
        getter = manage_blueprint_node("BP_Player", "create", node_type="Get Health")
        manage_blueprint_node("BP_Player", "move", node_id=getter["node_id"], position=[0, 100])
        manage_blueprint_node("BP_Player", "connect",
                            source_node_id=getter["node_id"], source_pin="Health",
                            target_node_id="Branch_1", target_pin="Condition")
        ```
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
        """
        🔧 **MULTI-ACTION BLUEPRINT FUNCTION MANAGER**: Complete Blueprint function lifecycle management.
        
        ## Supported Actions:
        - **create**: Create new custom functions (requires function_name)
        - **delete**: Remove functions (requires function_name)
        - **rename**: Rename functions (requires function_name, new_name)
        - **add_param**: Add input/output parameters (requires function_name, param_name, direction, type)
        - **remove_param**: Remove parameters (requires function_name, param_name)
        - **modify_param**: Change parameter properties (requires function_name, param_name, new_type/new_name)
        - **list**: List all functions in Blueprint
        - **get_info**: Get detailed function information (requires function_name)
        
        ## Function Development Workflow:
        
        ### Pattern 1: Function Creation → Parameter Setup → Node Population
        ```python
        # 1. Create function
        manage_blueprint_function("BP_Player", action="create", function_name="CalculateHealth")
        
        # 2. Add input parameters
        manage_blueprint_function("BP_Player", action="add_param", function_name="CalculateHealth",
                                param_name="BaseHealth", direction="input", type="float")
        manage_blueprint_function("BP_Player", action="add_param", function_name="CalculateHealth",
                                param_name="Modifier", direction="input", type="float")
        
        # 3. Add output parameter
        manage_blueprint_function("BP_Player", action="add_param", function_name="CalculateHealth",
                                param_name="FinalHealth", direction="output", type="float")
        
        # 4. Now populate with nodes using manage_blueprint_node
        manage_blueprint_node("BP_Player", action="create", graph_scope="function",
                            function_name="CalculateHealth", node_type="Multiply")
        ```
        
        ### Pattern 2: Function Parameter Management
        ```python
        # Add various parameter types
        manage_blueprint_function("BP_Player", "add_param", function_name="ProcessInput",
                                param_name="InputKey", direction="input", type="string")
        manage_blueprint_function("BP_Player", "add_param", function_name="ProcessInput", 
                                param_name="IsValid", direction="output", type="bool")
        manage_blueprint_function("BP_Player", "add_param", function_name="ProcessInput",
                                param_name="Execute", direction="exec", type="exec")
        ```
        
        ## Parameter Direction Types:
        - **"input"**: Function input parameters (appear on left side of function node)
        - **"output"**: Function return values (appear on right side of function node)
        - **"exec"**: Execution pins for control flow (in/out execution paths)
        
        ## Common Parameter Types:
        - **Basic**: "int", "float", "bool", "string"
        - **Objects**: "AActor", "UStaticMeshComponent", "UUserWidget"
        - **Containers**: "TArray<int>", "TMap<string,float>"
        - **Custom**: Blueprint class names (e.g., "BP_PlayerCharacter")
        - **Execution**: "exec" for control flow pins
        
        ## Action-Specific Parameters:
        
        **create**: function_name
        **delete**: function_name
        **rename**: function_name, new_name
        **add_param**: function_name, param_name, direction, type
        **remove_param**: function_name, param_name
        **modify_param**: function_name, param_name, [new_type and/or new_name]
        **list**: (no additional parameters)
        **get_info**: function_name
        
        ## Function Types & Properties:
        Functions can have additional properties set via the properties parameter:
        - **CallInEditor**: true/false - Allow function to be called in editor
        - **BlueprintPure**: true/false - Pure function (no execution pins)
        - **Category**: "Combat|Health" - Function category in Blueprint palette
        
        🎯 **Integration with Node Management**:
        After creating functions with parameters, use manage_blueprint_node with graph_scope="function"
        and function_name to add nodes to the function graph.
        
        ⚠️ **CRITICAL WORKFLOW**: Functions must be created before they can be used in manage_blueprint_node.
        Always create functions first, add parameters, then populate with nodes.
        """

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
