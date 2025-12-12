"""
UMG Widget Manager - Unified Multi-Action Tool

CONSOLIDATED UMG MANAGEMENT SYSTEM FOR AI ASSISTANTS

This module provides a single unified tool for ALL UMG Widget Blueprint operations,
following the successful multi-action pattern established by:
- manage_blueprint_node (node operations)
- manage_blueprint_function (function operations)
- manage_blueprint_variable (variable operations)
- manage_blueprint_component (component operations)

CRITICAL FOR AI WORKFLOW:
1. Use manage_umg_widget() for ALL UMG operations
2. Action parameter determines which operation to perform
3. Only provide parameters relevant to the specific action
4. Use get_help(topic="umg-guide") for styling and workflow best practices

ACTION CATEGORIES:
- Component Lifecycle: list_components, add_component, remove_component, validate
- Type Discovery: search_types, get_component_properties
- Property Management: get_property, set_property, list_properties
- Event Management: get_available_events, bind_events

BENEFITS:
- Single tool to learn instead of 15+ individual tools
- Consistent patterns with other manage_* tools
- Clear action-based organization
- Unified error handling and response format
"""

import logging
from typing import Dict, Any, List, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_umg_tools(mcp: FastMCP):
    """Register unified UMG management tool with the MCP server."""

    @mcp.tool(description="UMG Widget Blueprint management: add/remove components, set properties, bind events. Actions: list_components, add_component, remove_component, validate, search_types, get_component_properties, get_property, set_property, list_properties, get_available_events, bind_events. Use action='help' for all actions and detailed parameter info.")
    def manage_umg_widget(
        ctx: Context,
        action: str,
        help_action: str = "",
        widget_name: str = "",
        
        # Component operations
        component_name: str = "",
        component_type: str = "",
        parent_name: str = "root",
        
        # Property operations
        property_name: str = "",
        property_value: Optional[Any] = None,
        property_type: str = "auto",
        
        # Component creation options
        is_variable: bool = True,
        properties: Optional[Dict[str, Any]] = None,
        
        # Removal options
        remove_children: bool = True,
        remove_from_variables: bool = True,
        
        # Discovery/search options
        category: str = "",
        search_text: str = "",
        include_custom: bool = True,
        include_engine: bool = True,
        parent_compatibility: str = "",
        
        # Property listing options
        include_inherited: bool = True,
        category_filter: str = "",
        
        # Event operations
        input_events: Optional[Dict[str, str]] = None,
        
        # General options
        options: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """Route to UMG widget action handlers."""
        # Handle help action
        if action and action.lower() == "help":
            from help_system import generate_help_response
            return generate_help_response("manage_umg_widget", help_action if help_action else None)
        
        from vibe_ue_server import get_unreal_connection
        
        try:
            # Normalize action to lowercase
            action = action.lower().strip()
            
            # Validate action
            valid_actions = [
                "list_components", "add_component", "remove_component", "validate",
                "search_types", "get_component_properties",
                "get_property", "set_property", "list_properties",
                "get_available_events", "bind_events"
            ]
            
            # Import error response helper
            from help_system import generate_error_response
            
            if action not in valid_actions:
                error_response = generate_error_response(
                    "manage_umg_widget", action,
                    f"Invalid action '{action}'. Valid actions: {', '.join(valid_actions)}"
                )
                # Override available_actions with actual valid actions list
                error_response["available_actions"] = valid_actions
                return error_response
            
            # Action-specific validation before routing
            missing = []
            
            # Most actions require widget_name
            widget_required_actions = ["list_components", "add_component", "remove_component", "validate",
                                       "get_component_properties", "get_property", "set_property",
                                       "list_properties", "get_available_events", "bind_events"]
            
            if action in widget_required_actions and not widget_name:
                return generate_error_response(
                    "manage_umg_widget", action,
                    f"{action} requires 'widget_name' (full path like /Game/UI/WBP_MainMenu)",
                    missing_params=["widget_name"]
                )
            
            # Component operations validation
            if action == "add_component":
                if not component_type:
                    missing.append("component_type")
                if not component_name:
                    missing.append("component_name")
                if missing:
                    return generate_error_response(
                        "manage_umg_widget", action,
                        f"add_component requires: {', '.join(missing)}. Use search_types to find available component types.",
                        missing_params=missing
                    )
            
            elif action == "remove_component":
                if not component_name:
                    return generate_error_response(
                        "manage_umg_widget", action,
                        "remove_component requires 'component_name'",
                        missing_params=["component_name"]
                    )
            
            elif action in ["get_component_properties", "list_properties", "get_available_events"]:
                if not component_name:
                    return generate_error_response(
                        "manage_umg_widget", action,
                        f"{action} requires 'component_name'",
                        missing_params=["component_name"]
                    )
            
            elif action == "get_property":
                if not component_name:
                    missing.append("component_name")
                if not property_name:
                    missing.append("property_name")
                if missing:
                    return generate_error_response(
                        "manage_umg_widget", action,
                        f"get_property requires: {', '.join(missing)}",
                        missing_params=missing
                    )
            
            elif action == "set_property":
                if not component_name:
                    missing.append("component_name")
                if not property_name:
                    missing.append("property_name")
                if property_value is None:
                    missing.append("property_value")
                if missing:
                    return generate_error_response(
                        "manage_umg_widget", action,
                        f"set_property requires: {', '.join(missing)}",
                        missing_params=missing
                    )
            
            elif action == "bind_events":
                if not component_name:
                    missing.append("component_name")
                if not input_events:
                    missing.append("input_events")
                if missing:
                    return generate_error_response(
                        "manage_umg_widget", action,
                        f"bind_events requires: {', '.join(missing)}",
                        missing_params=missing
                    )
            
            # Route to appropriate handler
            if action == "list_components":
                return _handle_list_components(widget_name)
            
            elif action == "add_component":
                return _handle_add_component(
                    widget_name, component_type, component_name, parent_name,
                    is_variable, properties
                )
            
            elif action == "remove_component":
                return _handle_remove_component(
                    widget_name, component_name, remove_children, remove_from_variables
                )
            
            elif action == "validate":
                return _handle_validate(widget_name)
            
            elif action == "search_types":
                return _handle_search_types(
                    category, search_text, include_custom, include_engine, parent_compatibility
                )
            
            elif action == "get_component_properties":
                return _handle_get_component_properties(widget_name, component_name)
            
            elif action == "get_property":
                return _handle_get_property(widget_name, component_name, property_name)
            
            elif action == "set_property":
                return _handle_set_property(
                    widget_name, component_name, property_name, property_value, property_type
                )
            
            elif action == "list_properties":
                return _handle_list_properties(
                    widget_name, component_name, include_inherited, category_filter
                )
            
            elif action == "get_available_events":
                return _handle_get_available_events(widget_name, component_name)
            
            elif action == "bind_events":
                return _handle_bind_events(widget_name, component_name, input_events)
            
            else:
                # Should not be reached due to validation, but kept for safety
                return generate_error_response(
                    "manage_umg_widget", action,
                    f"Action '{action}' not implemented yet"
                )
                
        except Exception as e:
            error_msg = f"Error in manage_umg_widget (action={action}): {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}


# ============================================================================
# Component Lifecycle Action Handlers
# ============================================================================

def _handle_list_components(widget_name: str) -> Dict[str, Any]:
    """Handle list_components action."""
    from vibe_ue_server import get_unreal_connection
    
    if not widget_name:
        from help_system import generate_error_response
        return generate_error_response(
            tool_name="manage_umg_widget",
            action="list_components",
            error_message="widget_name is required for list_components action",
            missing_params=["widget_name"]
        )
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Listing components for widget '{widget_name}'")
        response = unreal.send_command("list_widget_components", {"widget_name": widget_name})
        
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"List components response: {response}")
        return response
        
    except Exception as e:
        error_msg = f"Error listing components: {e}"
        logger.error(error_msg)
        return {"success": False, "error": error_msg}


def _handle_add_component(
    widget_name: str,
    component_type: str,
    component_name: str,
    parent_name: str,
    is_variable: bool,
    properties: Optional[Dict[str, Any]]
) -> Dict[str, Any]:
    """Handle add_component action."""
    from vibe_ue_server import get_unreal_connection
    
    if not widget_name:
        return {"success": False, "error": "widget_name is required for add_component action"}
    if not component_type:
        return {"success": False, "error": "component_type is required for add_component action"}
    if not component_name:
        return {"success": False, "error": "component_name is required for add_component action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        params = {
            "widget_name": widget_name,
            "component_type": component_type,
            "component_name": component_name,
            "parent_name": parent_name,
            "is_variable": is_variable,
            "properties": properties or {}
        }
        
        logger.info(f"Adding component '{component_name}' of type '{component_type}' to widget '{widget_name}'")
        response = unreal.send_command("add_widget_component", params)
        
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"Add component response: {response}")
        return response
        
    except Exception as e:
        error_msg = f"Error adding component: {e}"
        logger.error(error_msg)
        return {"success": False, "error": error_msg}


def _handle_remove_component(
    widget_name: str,
    component_name: str,
    remove_children: bool,
    remove_from_variables: bool
) -> Dict[str, Any]:
    """Handle remove_component action."""
    from vibe_ue_server import get_unreal_connection
    
    if not widget_name:
        return {"success": False, "error": "widget_name is required for remove_component action"}
    if not component_name:
        return {"success": False, "error": "component_name is required for remove_component action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        params = {
            "widget_name": widget_name,
            "component_name": component_name,
            "remove_children": remove_children,
            "remove_from_variables": remove_from_variables
        }
        
        logger.info(f"Removing component '{component_name}' from widget '{widget_name}'")
        response = unreal.send_command("remove_umg_component", params)
        
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"Remove component response: {response}")
        return response
        
    except Exception as e:
        error_msg = f"Error removing component: {e}"
        logger.error(error_msg)
        return {"success": False, "error": error_msg}


def _handle_validate(widget_name: str) -> Dict[str, Any]:
    """Handle validate action."""
    from vibe_ue_server import get_unreal_connection
    
    if not widget_name:
        return {"success": False, "error": "widget_name is required for validate action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Validating widget hierarchy for '{widget_name}'")
        response = unreal.send_command("validate_widget_hierarchy", {"widget_name": widget_name})
        
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"Validate response: {response}")
        return response
        
    except Exception as e:
        error_msg = f"Error validating widget: {e}"
        logger.error(error_msg)
        return {"success": False, "error": error_msg}


# ============================================================================
# Type Discovery Action Handlers
# ============================================================================

def _handle_search_types(
    category: str,
    search_text: str,
    include_custom: bool,
    include_engine: bool,
    parent_compatibility: str
) -> Dict[str, Any]:
    """Handle search_types action."""
    from vibe_ue_server import get_unreal_connection
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        params = {
            "category": category,
            "include_custom": include_custom,
            "include_engine": include_engine,
            "parent_compatibility": parent_compatibility
        }
        
        logger.info(f"Searching widget types with params: {params}")
        response = unreal.send_command("get_available_widgets", params)
        
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}
        
        # Filter by search_text if provided (client-side filter)
        if search_text and response.get("success") and response.get("widgets"):
            search_lower = search_text.lower()
            filtered_widgets = [
                w for w in response["widgets"]
                if search_lower in w.get("name", "").lower() or
                   search_lower in w.get("display_name", "").lower()
            ]
            response["widgets"] = filtered_widgets
            response["count"] = len(filtered_widgets)
            response["filtered_by"] = search_text
        
        logger.info(f"Search types response: {response}")
        return response
        
    except Exception as e:
        error_msg = f"Error searching widget types: {e}"
        logger.error(error_msg)
        return {"success": False, "error": error_msg}


def _handle_get_component_properties(widget_name: str, component_name: str) -> Dict[str, Any]:
    """Handle get_component_properties action."""
    from vibe_ue_server import get_unreal_connection
    
    if not widget_name:
        return {"success": False, "error": "widget_name is required for get_component_properties action"}
    if not component_name:
        return {"success": False, "error": "component_name is required for get_component_properties action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        params = {
            "widget_name": widget_name,
            "component_name": component_name
        }
        
        logger.info(f"Getting properties for component '{component_name}' in widget '{widget_name}'")
        response = unreal.send_command("get_widget_component_properties", params)
        
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"Get component properties response: {response}")
        return response
        
    except Exception as e:
        error_msg = f"Error getting component properties: {e}"
        logger.error(error_msg)
        return {"success": False, "error": error_msg}


# ============================================================================
# Property Management Action Handlers
# ============================================================================

def _handle_get_property(widget_name: str, component_name: str, property_name: str) -> Dict[str, Any]:
    """Handle get_property action."""
    from vibe_ue_server import get_unreal_connection
    
    if not widget_name:
        return {"success": False, "error": "widget_name is required for get_property action"}
    if not component_name:
        return {"success": False, "error": "component_name is required for get_property action"}
    if not property_name:
        return {"success": False, "error": "property_name is required for get_property action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        params = {
            "widget_name": widget_name,
            "component_name": component_name,
            "property_name": property_name
        }
        
        logger.info(f"Getting property '{property_name}' from component '{component_name}' in widget '{widget_name}'")
        response = unreal.send_command("get_widget_property", params)
        
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"Get property response: {response}")
        return response
        
    except Exception as e:
        error_msg = f"Error getting property: {e}"
        logger.error(error_msg)
        return {"success": False, "error": error_msg}


def _handle_set_property(
    widget_name: str,
    component_name: str,
    property_name: str,
    property_value: Any,
    property_type: str
) -> Dict[str, Any]:
    """Handle set_property action."""
    from vibe_ue_server import get_unreal_connection
    
    if not widget_name:
        return {"success": False, "error": "widget_name is required for set_property action"}
    if not component_name:
        return {"success": False, "error": "component_name is required for set_property action"}
    if not property_name:
        return {"success": False, "error": "property_name is required for set_property action"}
    if property_value is None:
        return {"success": False, "error": "property_value is required for set_property action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        params = {
            "widget_name": widget_name,
            "component_name": component_name,
            "property_name": property_name,
            "property_value": property_value,
            "property_type": property_type
        }
        
        logger.info(f"Setting property '{property_name}' on component '{component_name}' in widget '{widget_name}' to '{property_value}'")
        response = unreal.send_command("set_widget_property", params)
        
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"Set property response: {response}")
        return response
        
    except Exception as e:
        error_msg = f"Error setting property: {e}"
        logger.error(error_msg)
        return {"success": False, "error": error_msg}


def _handle_list_properties(
    widget_name: str,
    component_name: str,
    include_inherited: bool,
    category_filter: str
) -> Dict[str, Any]:
    """Handle list_properties action."""
    from vibe_ue_server import get_unreal_connection
    
    if not widget_name:
        return {"success": False, "error": "widget_name is required for list_properties action"}
    if not component_name:
        return {"success": False, "error": "component_name is required for list_properties action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        params = {
            "widget_name": widget_name,
            "component_name": component_name,
            "include_inherited": include_inherited,
            "category_filter": category_filter
        }
        
        logger.info(f"Listing properties for component '{component_name}' in widget '{widget_name}'")
        response = unreal.send_command("list_widget_properties", params)
        
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"List properties response: {response}")
        return response
        
    except Exception as e:
        error_msg = f"Error listing properties: {e}"
        logger.error(error_msg)
        return {"success": False, "error": error_msg}


# ============================================================================
# Event Management Action Handlers
# ============================================================================

def _handle_get_available_events(widget_name: str, component_name: str) -> Dict[str, Any]:
    """Handle get_available_events action."""
    from vibe_ue_server import get_unreal_connection
    
    if not widget_name:
        return {"success": False, "error": "widget_name is required for get_available_events action"}
    if not component_name:
        return {"success": False, "error": "component_name is required for get_available_events action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        params = {
            "widget_name": widget_name,
            "component_name": component_name
        }
        
        logger.info(f"Getting available events for component '{component_name}' in widget '{widget_name}'")
        response = unreal.send_command("get_available_events", params)
        
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"Get available events response: {response}")
        return response
        
    except Exception as e:
        error_msg = f"Error getting available events: {e}"
        logger.error(error_msg)
        return {"success": False, "error": error_msg}


def _handle_bind_events(widget_name: str, component_name: str, input_events: Optional[Dict[str, str]]) -> Dict[str, Any]:
    """Handle bind_events action."""
    from vibe_ue_server import get_unreal_connection
    
    if not widget_name:
        return {"success": False, "error": "widget_name is required for bind_events action"}
    if not component_name:
        return {"success": False, "error": "component_name is required for bind_events action"}
    if not input_events:
        return {"success": False, "error": "input_events is required for bind_events action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        params = {
            "widget_name": widget_name,
            "component_name": component_name,
            "input_events": input_events
        }
        
        logger.info(f"Binding events for component '{component_name}' in widget '{widget_name}': {input_events}")
        response = unreal.send_command("bind_input_events", params)
        
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"Bind events response: {response}")
        return response
        
    except Exception as e:
        error_msg = f"Error binding events: {e}"
        logger.error(error_msg)
        return {"success": False, "error": error_msg}
