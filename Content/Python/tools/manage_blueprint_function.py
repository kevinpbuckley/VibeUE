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

    @mcp_instance.tool(description="Blueprint function management: create, delete, params, locals. Actions: create, delete, list, list_params, add_param, remove_param, update_param, add_local_var, remove_local_var, update_local_var, list_local_vars. Use action='help' for all actions and detailed parameter info. For add_param: use param_name, direction, type. For add_local_var: use local_name and type. For update_local_var: use local_name and new_type.")
    def manage_blueprint_function(
        help_action: str = "",
        blueprint_name: str = "",
        action: str = "",
        function_name: str = "",
        param_name: str = "",
        local_name: str = "",
        direction: str = "",
        type: str = "",
        new_type: str = "",
        new_name: str = "",
        properties=None,
        extra=None,
    ) -> Dict[str, Any]:
        """Blueprint function lifecycle and parameter management.
        
        **Available Actions:**
        - create, delete, list - Function CRUD
        - list_params, add_param, remove_param, update_param - Parameter management
        - list_local_vars, add_local_var, remove_local_var, update_local_var - Local variable management
        
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
        
        **update_local_var action:**
        Use to change the type of a local variable without removing/recreating it.
        Example: `manage_blueprint_function(action="update_local_var", blueprint_name="...", 
                  function_name="MyFunc", local_name="TempVar", new_type="int")`
        
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
        3. Direction can be `"input"`/`"in"` or `"output"`/`"out"` (all forms accepted)
        4. Type `"real"` from list_params should be `"float"` when adding params
        5. Object types require `"object:"` prefix: `"object:ABP_Enemy_C"`
        6. Use full Blueprint paths: `"/Game/Blueprints/BP_Player"` not `"BP_Player"`
        7. Use `update_local_var` to change local variable types instead of remove+add
        """
        
        # Handle help action
        if action and action.lower() == "help":
            from help_system import generate_help_response
            return generate_help_response("manage_blueprint_function", help_action if help_action else None)
        
        # Import error response helper
        from help_system import generate_error_response
        
        # Validate action is provided
        if not action:
            return generate_error_response(
                "manage_blueprint_function", "",
                "action is required. Available actions: create, delete, list, list_params, add_param, remove_param, update_param, add_local_var, remove_local_var, update_local_var, list_local_vars"
            )
        
        action_lower = action.lower()
        
        # Define valid actions
        valid_actions = [
            "create", "delete", "list", "list_params",
            "add_param", "remove_param", "update_param",
            "add_local_var", "remove_local_var", "update_local_var", "list_local_vars"
        ]
        
        if action_lower not in valid_actions:
            return generate_error_response(
                "manage_blueprint_function", action,
                f"Invalid action '{action}'. Valid actions: {', '.join(valid_actions)}"
            )
        
        # Action-specific validation
        missing = []
        
        if action_lower == "create":
            if not blueprint_name:
                missing.append("blueprint_name")
            if not function_name:
                missing.append("function_name")
            if missing:
                return generate_error_response(
                    "manage_blueprint_function", action,
                    f"create requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
        elif action_lower == "delete":
            if not blueprint_name:
                missing.append("blueprint_name")
            if not function_name:
                missing.append("function_name")
            if missing:
                return generate_error_response(
                    "manage_blueprint_function", action,
                    f"delete requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
        elif action_lower == "list":
            if not blueprint_name:
                return generate_error_response(
                    "manage_blueprint_function", action,
                    "list requires 'blueprint_name'",
                    missing_params=["blueprint_name"]
                )
        
        elif action_lower == "list_params":
            if not blueprint_name:
                missing.append("blueprint_name")
            if not function_name:
                missing.append("function_name")
            if missing:
                return generate_error_response(
                    "manage_blueprint_function", action,
                    f"list_params requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
        elif action_lower == "add_param":
            if not blueprint_name:
                missing.append("blueprint_name")
            if not function_name:
                missing.append("function_name")
            if not param_name:
                missing.append("param_name")
            if not type:
                missing.append("type")
            if not direction:
                missing.append("direction")
            if missing:
                return generate_error_response(
                    "manage_blueprint_function", action,
                    f"add_param requires: {', '.join(missing)}",
                    missing_params=missing
                )
            # Normalize direction: accept input/in and output/out
            direction_lower = direction.lower() if direction else ""
            if direction_lower in ["output", "out"]:
                direction = "out"
            elif direction_lower in ["input", "in"]:
                direction = "input"
            else:
                return generate_error_response(
                    "manage_blueprint_function", action,
                    f"direction must be 'input'/'in' or 'output'/'out'. Got: '{direction}'"
                )
        
        elif action_lower == "remove_param":
            if not blueprint_name:
                missing.append("blueprint_name")
            if not function_name:
                missing.append("function_name")
            if not param_name:
                missing.append("param_name")
            if missing:
                return generate_error_response(
                    "manage_blueprint_function", action,
                    f"remove_param requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
        elif action_lower == "update_param":
            if not blueprint_name:
                missing.append("blueprint_name")
            if not function_name:
                missing.append("function_name")
            if not param_name:
                missing.append("param_name")
            if missing:
                return generate_error_response(
                    "manage_blueprint_function", action,
                    f"update_param requires: {', '.join(missing)} plus new_type or new_name",
                    missing_params=missing
                )
        
        elif action_lower in ["add_local_var", "remove_local_var"]:
            if not blueprint_name:
                missing.append("blueprint_name")
            if not function_name:
                missing.append("function_name")
            # Accept either local_name or param_name for the variable name
            effective_local_name = local_name or param_name
            if not effective_local_name:
                missing.append("local_name")
            if action_lower == "add_local_var" and not type:
                missing.append("type")
            if missing:
                return generate_error_response(
                    "manage_blueprint_function", action,
                    f"{action} requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
        elif action_lower == "update_local_var":
            if not blueprint_name:
                missing.append("blueprint_name")
            if not function_name:
                missing.append("function_name")
            # Accept either local_name or param_name for the variable name
            effective_local_name = local_name or param_name
            if not effective_local_name:
                missing.append("local_name")
            if not new_type:
                missing.append("new_type")
            if missing:
                return generate_error_response(
                    "manage_blueprint_function", action,
                    f"update_local_var requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
        elif action_lower == "list_local_vars":
            if not blueprint_name:
                missing.append("blueprint_name")
            if not function_name:
                missing.append("function_name")
            if missing:
                return generate_error_response(
                    "manage_blueprint_function", action,
                    f"list_local_vars requires: {', '.join(missing)}",
                    missing_params=missing
                )

        payload: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "action": action,
        }

        # Build source dict with all optional parameters
        source: Dict[str, Any] = {
            "function_name": function_name,
            "param_name": param_name,
            "local_name": local_name,
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
