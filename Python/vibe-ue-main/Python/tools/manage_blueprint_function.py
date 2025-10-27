"""
Blueprint Function Management Tool

Multi-action tool for complete Blueprint function lifecycle management.
Consolidates all function operations (create, delete, parameters, locals) into a single tool.
"""

import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP

logger = logging.getLogger("vibeue.tools.manage_blueprint_function")


def _dispatch(tool_name: str, payload: Dict[str, Any]) -> Dict[str, Any]:
    """Dispatch a command to Unreal Engine via the socket client."""
    from vibe_ue_server import get_unreal_connection
    
    unreal = get_unreal_connection()
    if not unreal:
        logger.error("Failed to connect to Unreal Engine")
        return {"success": False, "error": "Failed to connect to Unreal Engine"}
    
    logger.info(f"Dispatching {tool_name} with payload: {payload}")
    response = unreal.send_command(tool_name, payload)
    if not response:
        logger.error("No response from Unreal Engine")
        return {"success": False, "error": "No response from Unreal Engine"}
    
    logger.debug(f"{tool_name} response: {response}")
    return response


def _merge(target: Dict[str, Any], source: Dict[str, Any]) -> None:
    """Merge non-None values from source into target."""
    for k, v in source.items():
        if v is not None:
            target[k] = v


def register_blueprint_function_tools(mcp_instance: FastMCP) -> None:
    """Register Blueprint function management tool."""

    @mcp_instance.tool()
    def manage_blueprint_function(
        blueprint_name: str = None,
        action: str = None,
        function_name: str = None,
        param_name: str = None,
        direction: str = None,
        type: str = None,
        new_type: str = None,
        new_name: str = None,
        properties: Dict[str, Any] = None,
        extra: Dict[str, Any] = None,
    ) -> Dict[str, Any]:
        """
         **MULTI-ACTION BLUEPRINT FUNCTION MANAGER**: Complete Blueprint function lifecycle management.
        
        ️ **CRITICAL: All action names are case-insensitive and processed as lowercase by C++ backend**
        
        ##  Complete Action Reference (All Available Actions):
        
        ### Discovery & Inspection Actions
        
        **list** - List all functions in Blueprint
        ```python
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="list"
        )
        # Returns: {"functions": [{"name": "FuncName", "node_count": 5}, ...]}
        ```
        
        **get** - Get detailed function information (️ NOT "get_info"!)
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
            direction="input",  # ️ Use "input" or "out" (NOT "output"!)
            type="float"
        )
        
        # Add output parameter (️ direction must be "out" not "output"!)
        manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="add_param",
            function_name="CalculateHealth",
            param_name="ResultHealth",
            direction="out",  #  CORRECT: "out" for output parameters
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
            param_name="TempResult",  # ️ Uses param_name for local variable name
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
        
        ##  Complete Function Recreation Workflow:
        
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
                    direction=param["direction"],  # ️ Use exact value from list_params
                    type=param["type"]
                )
        
        # Step 4: Verify parameters match
        new_params = manage_blueprint_function(
            blueprint_name="/Game/Blueprints/BP_Player2",
            action="list_params",
            function_name="CalculateHealth"
        )
        ```
        
        ## ️ Critical Parameter Direction Values:
        
        **Accepted Values (case-insensitive):**
        - **"input"** - Input parameters (left side of function node)
        - **"out"** - Output parameters (right side of function node)  USE THIS, NOT "output"!
        - **"return"** - Return value (alternative to "out")
        
        **Common Mistakes:**
        -  **"output"** - NOT VALID! Will cause "Invalid direction" error
        -  **"out"** - CORRECT for output parameters
        -  **"input"** - CORRECT for input parameters
        
        ##  Parameter Type Format Reference:
        
        **Primitive Types:**
        - `"int"`, `"float"`, `"bool"`, `"string"`, `"byte"`, `"name"`
        - ️ **"real"** from list_params → use **"float"** when adding params
        
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
        
        ##  Response Format Examples:
        
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
        
        ##  Integration with Node Management:
        
        After creating functions with parameters, use manage_blueprint_node with graph_scope="function"
        and function_name to add nodes to the function graph.
        
        ## ️ CRITICAL WORKFLOW ORDER:
        
        1. **Variables First**: Create all Blueprint variables
        2. **Functions Second**: Create functions and add parameters
        3. **Local Variables Third**: Add function-local variables
        4. **Nodes Last**: Add nodes to function graphs
        
        **Why this order matters:** Nodes that reference non-existent variables/functions will fail!
        
        ##  Pro Tips:
        
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

    logger.info("Blueprint function management tool registered")
