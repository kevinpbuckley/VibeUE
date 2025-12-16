"""
Blueprint Variable Management Tool

This module provides unified variable management for Blueprint assets.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_blueprint_variable_tools(mcp: FastMCP):
    """Register Blueprint variable management tools with the MCP server."""
    
    @mcp.tool(description="""Blueprint variable operations: create, delete, get/set properties, diagnostics. Actions: create, delete, list, get_info, modify, search_types, diagnostics, get_property, set_property, get_property_metadata, set_property_metadata. Use action='help' for all actions and detailed parameter info. For create: use search_types to discover type_path, or use aliases: 'float', 'double', 'int', 'int64', 'bool', 'string', 'name', 'text', 'byte'.""")
    def manage_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        action: str,
        help_action: Optional[str] = None,
        variable_name: Optional[str] = None,
        variable_config: Optional[Dict[str, Any]] = None,
        property_path: Optional[str] = None,
        value: Optional[Any] = None,
        delete_options: Optional[Dict[str, Any]] = None,
        list_criteria: Optional[Dict[str, Any]] = None,
        info_options: Optional[Dict[str, Any]] = None,
        search_criteria: Optional[Dict[str, Any]] = None,
        search_text: Optional[str] = None,
        options: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """Route to Blueprint variable action handlers."""
        from vibe_ue_server import get_unreal_connection
        
        try:
            # Handle help action
            if action == "help":
                from help_system import generate_help_response
                return generate_help_response("manage_blueprint_variable", help_action)
            
            # Import error response helper
            from help_system import generate_error_response
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Validate action
            valid_actions = [
                "create", "delete", "list", "get_info", "modify", "search_types",
                "diagnostics", "get_property", "set_property", "get_property_metadata", "set_property_metadata"
            ]
            
            if action not in valid_actions:
                return generate_error_response(
                    "manage_blueprint_variable", action,
                    f"Invalid action '{action}'. Valid actions: {', '.join(valid_actions)}"
                )
            
            # Action-specific validation
            if action == "create":
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                if not variable_name:
                    missing.append("variable_name")
                if not variable_config:
                    missing.append("variable_config")
                if missing:
                    return generate_error_response(
                        "manage_blueprint_variable", action,
                        f"create requires: {', '.join(missing)}",
                        missing_params=missing
                    )
                
                # CRITICAL VALIDATION: Catch common "type" vs "type_path" mistake
                if "type" in variable_config and "type_path" not in variable_config:
                    return generate_error_response(
                        "manage_blueprint_variable", action,
                        "variable_config uses 'type' but MUST use 'type_path'. Use aliases like 'float', 'int', 'bool', 'string', 'vector' or search_types to find type paths.",
                        missing_params=["variable_config.type_path"]
                    )
                
                if "type_path" not in variable_config:
                    return generate_error_response(
                        "manage_blueprint_variable", action,
                        "variable_config missing 'type_path'. Use aliases like 'float', 'int', 'bool', 'string', 'vector' or search_types to find type paths.",
                        missing_params=["variable_config.type_path"]
                    )
            
            elif action == "delete":
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                if not variable_name:
                    missing.append("variable_name")
                if missing:
                    return generate_error_response(
                        "manage_blueprint_variable", action,
                        f"delete requires: {', '.join(missing)}",
                        missing_params=missing
                    )
            
            elif action == "list":
                if not blueprint_name:
                    return generate_error_response(
                        "manage_blueprint_variable", action,
                        "list requires 'blueprint_name'",
                        missing_params=["blueprint_name"]
                    )
            
            elif action == "get_info":
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                if not variable_name:
                    missing.append("variable_name")
                if missing:
                    return generate_error_response(
                        "manage_blueprint_variable", action,
                        f"get_info requires: {', '.join(missing)}",
                        missing_params=missing
                    )
            
            elif action in ["get_property", "set_property"]:
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                # property_path is the variable name itself (e.g., "Health" to set Health's default value)
                if not property_path:
                    missing.append("property_path")
                if action == "set_property" and value is None:
                    missing.append("value")
                if missing:
                    return generate_error_response(
                        "manage_blueprint_variable", action,
                        f"{action} requires: {', '.join(missing)}",
                        missing_params=missing
                    )
            
            # Build command parameters
            params: Dict[str, Any] = {
                "action": action,
                "blueprint_name": blueprint_name
            }
            
            # Add optional parameters based on action
            if variable_name:
                params["variable_name"] = variable_name
            if variable_config:
                params["variable_config"] = variable_config
            if property_path:
                params["property_path"] = property_path
                params["path"] = property_path
            if value is not None:
                params["value"] = value
            if delete_options:
                params["delete_options"] = delete_options
            if list_criteria:
                params["list_criteria"] = list_criteria
            if info_options:
                params["info_options"] = info_options
            if search_criteria:
                params["search_criteria"] = search_criteria
            if search_text:
                params["search_text"] = search_text
            if options:
                params["options"] = options
                
            response = unreal.send_command("manage_blueprint_variable", params)
            
            if not response:
                logger.error("No response from Unreal Engine for manage_blueprint_variable")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Unified variable management response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error in unified variable management: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    logger.info("Blueprint variable management tools registered")
