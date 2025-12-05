"""
Blueprint Component Management Tool

This module provides unified component management for Blueprint assets.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_blueprint_component_tools(mcp: FastMCP):
    """Register Blueprint component management tools with the MCP server."""
    
    @mcp.tool(description="Blueprint component operations: create, delete, get/set properties, hierarchy. Actions: search_types, list, create, delete, get_property, set_property, get_all_properties, reparent, reorder. CRITICAL: Use full package paths like /Game/Blueprints/BP_Player (not short names). Use get_help(topic='blueprint-workflow') for examples.")
    def manage_blueprint_component(
        ctx: Context,
        blueprint_name: str,
        action: str,
        # Component identification
        component_type: str = "",
        component_name: str = "",
        # Property operations
        property_name: str = "",
        property_value: Any = None,
        # Component creation parameters
        parent_name: str = "",
        properties: Dict[str, Any] = None,
        location: List[float] = None,
        rotation: List[float] = None,
        scale: List[float] = None,
        # Hierarchy operations
        component_order: List[str] = None,
        remove_children: bool = True,
        # Discovery parameters
        category: str = "",
        base_class: str = "",
        search_text: str = "",
        include_abstract: bool = False,
        include_deprecated: bool = False,
        include_property_values: bool = False,
        include_inherited: bool = True,
        # Additional options
        options: Dict[str, Any] = None
    ) -> Dict[str, Any]:
        """Route to Blueprint component action handlers."""
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Validate action
            valid_actions = [
                "search_types", "get_info", "get_property_metadata", "list",
                "create", "delete",
                "get_property", "set_property", "get_all_properties", "compare_properties",
                "reorder", "reparent"
            ]
            
            if action not in valid_actions:
                return {
                    "success": False,
                    "message": f"Invalid action '{action}'. Valid actions: {', '.join(valid_actions)}"
                }
            
            # Route to appropriate backend command
            
            if action == "search_types":
                params = {
                    "category": category,
                    "base_class": base_class,
                    "search_text": search_text,
                    "include_abstract": include_abstract,
                    "include_deprecated": include_deprecated
                }
                response = unreal.send_command("get_available_components", params)
                
            elif action == "get_info":
                params = {
                    "component_type": component_type,
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "include_property_values": include_property_values
                }
                response = unreal.send_command("get_component_info", params)
                
            elif action == "get_property_metadata":
                params = {
                    "component_type": component_type,
                    "property_name": property_name
                }
                response = unreal.send_command("get_property_metadata", params)
                
            elif action == "list":
                params = {"blueprint_name": blueprint_name}
                response = unreal.send_command("get_component_hierarchy", params)
                
            elif action == "create":
                params = {
                    "blueprint_name": blueprint_name,
                    "component_type": component_type,
                    "component_name": component_name
                }
                if parent_name:
                    params["parent_name"] = parent_name
                if properties:
                    params["properties"] = properties
                if location:
                    params["location"] = [float(x) for x in location]
                if rotation:
                    params["rotation"] = [float(x) for x in rotation]
                if scale:
                    params["scale"] = [float(x) for x in scale]
                response = unreal.send_command("add_component", params)
                
            elif action == "delete":
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "remove_children": remove_children
                }
                response = unreal.send_command("remove_component", params)
                
            elif action == "get_property":
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "property_name": property_name
                }
                response = unreal.send_command("get_component_property", params)
                
            elif action == "set_property":
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "property_name": property_name,
                    "property_value": property_value
                }
                response = unreal.send_command("set_component_property", params)
                
            elif action == "get_all_properties":
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "include_inherited": include_inherited
                }
                response = unreal.send_command("get_all_component_properties", params)
                
            elif action == "compare_properties":
                if not options or "compare_to_blueprint" not in options or "compare_to_component" not in options:
                    return {
                        "success": False,
                        "message": "compare_properties requires options with 'compare_to_blueprint' and 'compare_to_component'"
                    }
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "compare_to_blueprint": options["compare_to_blueprint"],
                    "compare_to_component": options["compare_to_component"]
                }
                response = unreal.send_command("compare_component_properties", params)
                
            elif action == "reorder":
                params = {
                    "blueprint_name": blueprint_name,
                    "component_order": component_order
                }
                response = unreal.send_command("reorder_components", params)
                
            elif action == "reparent":
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "parent_name": parent_name
                }
                response = unreal.send_command("reparent_component", params)
            
            else:
                return {"success": False, "message": f"Action '{action}' not yet implemented"}
            
            if not response:
                logger.error(f"No response from Unreal Engine for component action: {action}")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Component management action '{action}' completed successfully")
            return response
            
        except Exception as e:
            error_msg = f"Error in component management (action={action}): {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    logger.info("Blueprint component management tools registered")
