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
    """Merge non-empty values from source into target."""
    for k, v in source.items():
        if v is not None and v != "":
            target[k] = v


def register_blueprint_function_tools(mcp_instance: FastMCP) -> None:
    """Register Blueprint function management tool."""

    @mcp_instance.tool(description="Blueprint function management: create, delete, params, locals. Use get_help('multi-action-tools') for action reference.")
    def manage_blueprint_function(
        blueprint_name: str = "",
        action: str = "",
        function_name: str = "",
        param_name: str = "",
        direction: str = "",
        type: str = "",
        new_type: str = "",
        new_name: str = "",
        properties=None,
        extra=None,
    ) -> Dict[str, Any]:
        """Blueprint function lifecycle and parameter management.
        
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
        
        ## CRITICAL WORKFLOW ORDER:
        
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

        # Build source dict with all optional parameters
        source: Dict[str, Any] = {
            "function_name": function_name,
            "param_name": param_name,
            "direction": direction,
            "type": type,
            "new_type": new_type,
            "new_name": new_name,
        }
        
        # Add properties if provided
        if properties is not None:
            source["properties"] = properties
            
        _merge(payload, source)

        # Merge extra parameters if provided
        if extra is not None:
            _merge(payload, extra)

        return _dispatch("manage_blueprint_function", payload)

    logger.info("Blueprint function management tool registered")
