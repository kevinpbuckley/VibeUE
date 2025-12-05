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
    
    @mcp.tool(description="Blueprint variable operations: create, delete, get/set properties. Actions: create, delete, get_info, list, search_types, get_property, set_property. CRITICAL: Use type_path not type (e.g., '/Script/CoreUObject.FloatProperty'). Use get_help(topic='multi-action-tools') for examples.")
    def manage_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        action: str,
        variable_name: Optional[str] = None,
        variable_config: Optional[Dict[str, Any]] = None,
        property_path: Optional[str] = None,
        value: Optional[Any] = None,
        delete_options: Optional[Dict[str, Any]] = None,
        list_criteria: Optional[Dict[str, Any]] = None,
        info_options: Optional[Dict[str, Any]] = None,
        search_criteria: Optional[Dict[str, Any]] = None,
        options: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """Route to Blueprint variable action handlers."""
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # ️ CRITICAL VALIDATION: Catch common "type" vs "type_path" mistake
            if action == "create" and variable_config:
                if "type" in variable_config and "type_path" not in variable_config:
                    error_msg = (
                        " CRITICAL ERROR: variable_config uses 'type' but should use 'type_path'\n"
                        f" WRONG: variable_config={variable_config}\n"
                        " CORRECT: Use 'type_path' key with full canonical path:\n"
                        "   Examples:\n"
                        '   - {"type_path": "/Script/CoreUObject.FloatProperty"} for float\n'
                        '   - {"type_path": "/Script/UMG.UserWidget"} for widgets\n'
                        '   - {"type_path": "/Script/Engine.SoundBase"} for audio\n'
                        "Use manage_blueprint_variable(action='search_types') to find correct type_path"
                    )
                    logger.error(error_msg)
                    return {"success": False, "message": error_msg}
                
                if "type_path" not in variable_config:
                    error_msg = (
                        " CRITICAL ERROR: variable_config missing required 'type_path' field\n"
                        f"Current variable_config: {variable_config}\n"
                        " REQUIRED: variable_config must include 'type_path' with full canonical path:\n"
                        "   Examples:\n"
                        '   - {"type_path": "/Script/CoreUObject.FloatProperty"} for float\n'
                        '   - {"type_path": "/Script/UMG.UserWidget"} for widgets\n'
                        "Use manage_blueprint_variable(action='search_types') to find correct type_path"
                    )
                    logger.error(error_msg)
                    return {"success": False, "message": error_msg}
            
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
