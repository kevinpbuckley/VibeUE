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
    
    @mcp.tool(description="Blueprint component operations: create, delete, get/set properties, hierarchy. Actions: search_types, get_info, get_property_metadata, list, create, delete, get_property, set_property, get_all_properties, compare_properties, reorder, reparent. Use action='help' for all actions and detailed parameter info. CRITICAL: Use full package paths like /Game/Blueprints/BP_Player (not short names).")
    def manage_blueprint_component(
        ctx: Context,
        blueprint_name: str,
        action: str,
        help_action: str = "",
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
        category: Optional[str] = "",
        base_class: Optional[str] = "",
        search_text: Optional[str] = "",
        include_abstract: bool = False,
        include_deprecated: bool = False,
        include_property_values: bool = False,
        include_inherited: bool = True,
        # Compare parameters - for compare_properties action
        other_blueprint: str = "",  # Blueprint containing the second component (defaults to blueprint_name)
        other_component: str = "",  # Second component to compare with
        # Additional options
        options: Dict[str, Any] = None
    ) -> Dict[str, Any]:
        """Route to Blueprint component action handlers."""
        from vibe_ue_server import get_unreal_connection
        
        try:
            # Handle help action
            if action == "help":
                from help_system import generate_help_response
                return generate_help_response("manage_blueprint_component", help_action if help_action else None)
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Import error response helper
            from help_system import generate_error_response
            
            # Validate action
            valid_actions = [
                "search_types", "get_info", "get_property_metadata", "list",
                "create", "delete",
                "get_property", "set_property", "get_all_properties", "compare_properties",
                "reorder", "reparent"
            ]
            
            if action not in valid_actions:
                return generate_error_response(
                    "manage_blueprint_component", action,
                    f"Invalid action '{action}'. Valid actions: {', '.join(valid_actions)}"
                )
            
            # Route to appropriate backend command
            
            # Normalize Optional[str] params to ensure they're never None
            category = category or ""
            base_class = base_class or ""
            search_text = search_text or ""
            
            if action == "search_types":
                # No required params for search_types
                params = {
                    "category": category,
                    "base_class": base_class,
                    "search_text": search_text,
                    "include_abstract": include_abstract,
                    "include_deprecated": include_deprecated
                }
                response = unreal.send_command("get_available_components", params)
                
            elif action == "get_info":
                # Requires either component_type OR (blueprint_name + component_name)
                if not component_type and (not blueprint_name or not component_name):
                    return generate_error_response(
                        "manage_blueprint_component", action,
                        "get_info requires either 'component_type' OR both 'blueprint_name' and 'component_name'",
                        missing_params=["component_type"] if not blueprint_name else ["component_name"]
                    )
                params = {
                    "component_type": component_type,
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "include_property_values": include_property_values
                }
                response = unreal.send_command("get_component_info", params)
                
            elif action == "get_property_metadata":
                if not component_type:
                    return generate_error_response(
                        "manage_blueprint_component", action,
                        "get_property_metadata requires 'component_type'",
                        missing_params=["component_type"]
                    )
                params = {
                    "component_type": component_type,
                    "property_name": property_name
                }
                response = unreal.send_command("get_property_metadata", params)
                
            elif action == "list":
                if not blueprint_name:
                    return generate_error_response(
                        "manage_blueprint_component", action,
                        "list requires 'blueprint_name'",
                        missing_params=["blueprint_name"]
                    )
                params = {"blueprint_name": blueprint_name}
                response = unreal.send_command("get_component_hierarchy", params)
                
            elif action == "create":
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                if not component_type:
                    missing.append("component_type")
                if not component_name:
                    missing.append("component_name")
                if missing:
                    return generate_error_response(
                        "manage_blueprint_component", action,
                        f"create requires: {', '.join(missing)}",
                        missing_params=missing
                    )
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
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                if not component_name:
                    missing.append("component_name")
                if missing:
                    return generate_error_response(
                        "manage_blueprint_component", action,
                        f"delete requires: {', '.join(missing)}",
                        missing_params=missing
                    )
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "remove_children": remove_children
                }
                response = unreal.send_command("remove_component", params)
                
            elif action == "get_property":
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                if not component_name:
                    missing.append("component_name")
                if not property_name:
                    missing.append("property_name")
                if missing:
                    return generate_error_response(
                        "manage_blueprint_component", action,
                        f"get_property requires: {', '.join(missing)}",
                        missing_params=missing
                    )
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "property_name": property_name
                }
                response = unreal.send_command("get_component_property", params)
                
            elif action == "set_property":
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                if not component_name:
                    missing.append("component_name")
                if not property_name:
                    missing.append("property_name")
                if property_value is None:
                    missing.append("property_value")
                if missing:
                    return generate_error_response(
                        "manage_blueprint_component", action,
                        f"set_property requires: {', '.join(missing)}",
                        missing_params=missing
                    )
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "property_name": property_name,
                    "property_value": property_value
                }
                response = unreal.send_command("set_component_property", params)
                
            elif action == "get_all_properties":
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                if not component_name:
                    missing.append("component_name")
                if missing:
                    return generate_error_response(
                        "manage_blueprint_component", action,
                        f"get_all_properties requires: {', '.join(missing)}",
                        missing_params=missing
                    )
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "include_inherited": include_inherited
                }
                response = unreal.send_command("get_all_component_properties", params)
                
            elif action == "compare_properties":
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                if not component_name:
                    missing.append("component_name")
                
                # Support explicit parameters (preferred) or options dict (legacy)
                # other_component: second component to compare
                # other_blueprint: blueprint containing second component (defaults to same blueprint)
                compare_component = other_component
                compare_blueprint = other_blueprint if other_blueprint else blueprint_name
                
                # Fallback to options dict for backwards compatibility
                if not compare_component and options:
                    if "other_component" in options:
                        compare_component = options["other_component"]
                    elif "compare_to_component" in options:
                        compare_component = options["compare_to_component"]
                    if "compare_to_blueprint" in options:
                        compare_blueprint = options["compare_to_blueprint"]
                    elif "other_blueprint" in options:
                        compare_blueprint = options["other_blueprint"]
                
                if not compare_component:
                    missing.append("other_component")
                    
                if missing:
                    return generate_error_response(
                        "manage_blueprint_component", action,
                        f"compare_properties requires: {', '.join(missing)}",
                        missing_params=missing
                    )
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "compare_to_blueprint": compare_blueprint,
                    "compare_to_component": compare_component
                }
                response = unreal.send_command("compare_component_properties", params)
                
            elif action == "reorder":
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                if not component_order:
                    missing.append("component_order")
                if missing:
                    return generate_error_response(
                        "manage_blueprint_component", action,
                        f"reorder requires: {', '.join(missing)}",
                        missing_params=missing
                    )
                params = {
                    "blueprint_name": blueprint_name,
                    "component_order": component_order
                }
                response = unreal.send_command("reorder_components", params)
                
            elif action == "reparent":
                missing = []
                if not blueprint_name:
                    missing.append("blueprint_name")
                if not component_name:
                    missing.append("component_name")
                if missing:
                    return generate_error_response(
                        "manage_blueprint_component", action,
                        f"reparent requires: {', '.join(missing)}",
                        missing_params=missing
                    )
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "parent_name": parent_name
                }
                response = unreal.send_command("reparent_component", params)
            
            else:
                return generate_error_response(
                    "manage_blueprint_component", action,
                    f"Action '{action}' not yet implemented"
                )
            
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
