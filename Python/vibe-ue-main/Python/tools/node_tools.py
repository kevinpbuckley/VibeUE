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
        ctx: Context = None,  # ✅ Made optional - framework should inject, but AI doesn't need to pass it
        blueprint_name: str = None,
        action: str = None,
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

        ✨ **External Targets Supported (Sept 2025)**: Supply the optional metadata fields below to spawn fully configured
        nodes for engine helpers and Blueprint casts without manual cleanup.
        - `node_params.function_name` + `node_params.function_class` (or `FunctionReference.MemberParent`) for
          static/global calls such as `UGameplayStatics::GetPlayerController`.
        - `node_params.cast_target` (soft class path or Blueprint class name) for `Cast To <Class>` nodes.
        The server now resolves these descriptors, loads the referenced class, and reconstructs the node so the expected
        pins appear immediately.

        ## Supported Actions:
        - **create**: Create new nodes (requires node_type from get_available_blueprint_nodes())
    - **connect**: Connect pins between nodes (requires source/target node_id and pin names)
    - **connect_pins**: Batch connect pins with schema validation, conversion helpers, and diagnostics
    - **disconnect**: Disconnect pins (requires source/target node_id and pin names)
    - **disconnect_pins**: Break specific links or clear entire pins using the new payload format
        - **delete**: Remove nodes (requires node_id)
        - **move**: Reposition nodes (requires node_id and position)
    - **list**: List all nodes in graph (returns node inventory)
    - **describe**: Rich node + pin metadata payload (deterministic ordering)
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
    **connect_pins**: connections array with source/target pin identifiers plus optional conversion flags
        **disconnect**: source_node_id, source_pin, target_node_id, target_pin
    **disconnect_pins**: connections array or pin_ids list defining which links to break
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
        search_term: str = "",
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
            search_term: Text filter for node names, descriptions, and keywords (like Unreal's Add Action search)
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
        
        # Search for "Play Sound" nodes like Unreal's Add Action menu
        get_available_blueprint_nodes("BP_Player", search_term="Play Sound")
        
        # Find mathematical operations
        get_available_blueprint_nodes("BP_Player", category="Math")
        
        # Search for specific functions - find all "Location" related nodes
        get_available_blueprint_nodes("BP_Player", search_term="Location")
        
        # Combined filtering: Audio category with "Play" in the name
        get_available_blueprint_nodes("BP_Player", category="Audio", search_term="Play")
        
        # Find variable operations
        get_available_blueprint_nodes("BP_Player", category="Variables")
        ```
        
        🎯 **AI Integration**: This tool enables data-driven Blueprint development by providing
        exact node names that work reliably, eliminating guesswork and trial-and-error.
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
        }

        return _dispatch("get_available_blueprint_nodes", payload)

    @mcp.tool()
    def manage_blueprint_function(
        ctx: Context = None,  # ✅ Made optional - framework should inject, but AI doesn't need to pass it
        blueprint_name: str = None,
        action: str = None,
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
        
        ⚠️ **CRITICAL: All action names are case-insensitive and processed as lowercase by C++ backend**
        
        ## 📋 Complete Action Reference (All Available Actions):
        
        ### Discovery & Inspection Actions
        
        **list** - List all functions in Blueprint
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="list"
        )
        # Returns: {"functions": [{"name": "FuncName", "node_count": 5}, ...]}
        ```
        
        **get** - Get detailed function information (⚠️ NOT "get_info"!)
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="get",
            function_name="CalculateHealth"
        )
        # Returns: {"name": "CalculateHealth", "node_count": 10, "graph_guid": "..."}
        ```
        
        **list_params** - List all function parameters (inputs, outputs, locals)
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="list_params",
            function_name="CalculateHealth"
        )
        # Returns: {"success": true, "function_name": "CalculateHealth", 
        #           "parameters": [{"name": "BaseHealth", "direction": "input", "type": "float"}, ...],
        #           "count": 2}
        ```
        
        ### Function Lifecycle Actions
        
        **create** - Create new custom function
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="create",
            function_name="CalculateHealth"
        )
        # Returns: {"success": true, "function_name": "CalculateHealth", "graph_guid": "..."}
        ```
        
        **delete** - Remove function from Blueprint
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="delete",
            function_name="OldFunction"
        )
        # Returns: {"success": true, "function_name": "OldFunction"}
        ```
        
        ### Parameter Management Actions
        
        **add_param** - Add input/output parameter to function
        ```python
        # Add input parameter
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="add_param",
            function_name="CalculateHealth",
            param_name="BaseHealth",
            direction="input",  # ⚠️ Use "input" or "out" (NOT "output"!)
            type="float"
        )
        
        # Add output parameter (⚠️ direction must be "out" not "output"!)
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="add_param",
            function_name="CalculateHealth",
            param_name="ResultHealth",
            direction="out",  # ✅ CORRECT: "out" for output parameters
            type="float"
        )
        
        # Add object reference parameter
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="add_param",
            function_name="ProcessActor",
            param_name="TargetActor",
            direction="input",
            type="object:ABP_Enemy_C"  # Format: "object:ClassName"
        )
        ```
        
        **remove_param** - Remove parameter from function
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="remove_param",
            function_name="CalculateHealth",
            param_name="OldParam",
            direction="input"  # Specify which direction to remove from
        )
        ```
        
        **update_param** - Update parameter type or name
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="update_param",
            function_name="CalculateHealth",
            param_name="OldParamName",
            direction="input",
            new_type="int",  # Optional: change type
            new_name="NewParamName"  # Optional: rename
        )
        ```
        
        ### Local Variable Actions
        
        **list_locals** - List all local variables in function (aliases: "locals", "list_local_vars")
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="list_locals",
            function_name="CalculateHealth"
        )
        ```
        
        **add_local** - Add local variable to function
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="add_local",
            function_name="CalculateHealth",
            param_name="TempResult",  # ⚠️ Uses param_name for local variable name
            type="float"
        )
        ```
        
        **remove_local** - Remove local variable from function
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="remove_local",
            function_name="CalculateHealth",
            param_name="TempResult"
        )
        ```
        
        **update_local** - Update local variable type
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="update_local",
            function_name="CalculateHealth",
            param_name="TempResult",
            new_type="int"
        )
        ```
        
        ### Function Properties Action
        
        **update_properties** - Update function metadata (pure, category, etc.)
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="update_properties",
            function_name="CalculateHealth",
            properties={
                "CallInEditor": true,
                "BlueprintPure": true,
                "Category": "Combat|Health"
            }
        )
        ```
        
        ## 🎯 Complete Function Recreation Workflow:
        
        ```python
        # Step 1: Discover original function structure
        original_params = manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="list_params",
            function_name="CalculateHealth"
        )
        
        # Step 2: Create new function
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player2",
            action="create",
            function_name="CalculateHealth"
        )
        
        # Step 3: Add all parameters from original
        for param in original_params["parameters"]:
            if param["name"] != "execute":  # Skip auto-generated exec pin
                manage_blueprint_function(
                    blueprint_name="/Game/Blueprints/BP_Player2",
                    action="add_param",
                    function_name="CalculateHealth",
                    param_name=param["name"],
                    direction=param["direction"],  # ⚠️ Use exact value from list_params
                    type=param["type"]
                )
        
        # Step 4: Verify parameters match
        new_params = manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player2",
            action="list_params",
            function_name="CalculateHealth"
        )
        ```
        
        ## ⚠️ Critical Parameter Direction Values:
        
        **Accepted Values (case-insensitive):**
        - **"input"** - Input parameters (left side of function node)
        - **"out"** - Output parameters (right side of function node) ✅ USE THIS, NOT "output"!
        - **"return"** - Return value (alternative to "out")
        
        **Common Mistakes:**
        - ❌ **"output"** - NOT VALID! Will cause "Invalid direction" error
        - ✅ **"out"** - CORRECT for output parameters
        - ✅ **"input"** - CORRECT for input parameters
        
        ## 📊 Parameter Type Format Reference:
        
        **Primitive Types:**
        - `"int"`, `"float"`, `"bool"`, `"string"`, `"byte"`, `"name"`
        - ⚠️ **"real"** from list_params → use **"float"** when adding params
        
        **Object References:**
        - Format: `"object:ClassName"`
        - Examples: `"object:AActor"`, `"object:UUserWidget"`, `"object:ABP_Enemy_C"`
        
        **Struct Types:**
        - Format: `"struct:StructName"`
        - Examples: `"struct:FVector"`, `"struct:FRotator"`
        
        **Container Types:**
        - Arrays: `"array<float>"`, `"array<object:AActor>"`
        - Maps: Not directly supported via simple type strings
        
        **Execution Flow:**
        - `"exec"` - Execution pins (automatically created for first output parameter)
        
        ## 🔍 Response Format Examples:
        
        **list action:**
        ```json
        {
            "functions": [
                {"name": "UserConstructionScript", "node_count": 1},
                {"name": "CalculateHealth", "node_count": 5},
                {"name": "ProcessInput", "node_count": 8}
            ]
        }
        ```
        
        **get action:**
        ```json
        {
            "name": "CalculateHealth",
            "node_count": 5,
            "graph_guid": "2A845B17413D8EE95756C99189A581D9"
        }
        ```
        
        **list_params action:**
        ```json
        {
            "success": true,
            "function_name": "CalculateHealth",
            "parameters": [
                {"name": "BaseHealth", "direction": "input", "type": "float"},
                {"name": "Modifier", "direction": "input", "type": "float"},
                {"name": "execute", "direction": "out", "type": "exec"},
                {"name": "ResultHealth", "direction": "out", "type": "float"}
            ],
            "count": 4
        }
        ```
        
        ## 🎯 Integration with Node Management:
        
        After creating functions with parameters, use manage_blueprint_node with graph_scope="function"
        and function_name to add nodes to the function graph.
        
        ## ⚠️ CRITICAL WORKFLOW ORDER:
        
        1. **Variables First**: Create all Blueprint variables
        2. **Functions Second**: Create functions and add parameters
        3. **Local Variables Third**: Add function-local variables
        4. **Nodes Last**: Add nodes to function graphs
        
        **Why this order matters:** Nodes that reference non-existent variables/functions will fail!
        
        ## 💡 Pro Tips:
        
        1. Always use `list_params` to discover original function signatures before recreation
        2. The `execute` output pin is auto-created when you add your first output parameter
        3. Direction must be exactly `"input"` or `"out"` (not "output"!)
        4. Type `"real"` from list_params should be `"float"` when adding params
        5. Object types require `"object:"` prefix: `"object:ABP_Enemy_C"`
        6. Use full Blueprint paths: `"/Game/Blueprints/BP_Player"` not `"BP_Player"`
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
