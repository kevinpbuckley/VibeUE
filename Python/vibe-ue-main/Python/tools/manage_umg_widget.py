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

    @mcp.tool()
    def manage_umg_widget(
        ctx: Context,
        action: str,
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
        """
         UMG Widget Manager Tool
        
        Single multi-action tool for ALL UMG Widget Blueprint operations
        
        **CRITICAL**: Before styling widgets, use get_help(topic="umg-guide") to understand
        proper container-specific patterns and widget hierarchy requirements.
        
        ##  Available Actions:
        
        ### Component Lifecycle Actions
        
        **list_components** - List all components with hierarchy and properties
        ```python
        manage_umg_widget(
            action="list_components",
            widget_name="WBP_Inventory"
        )
        ```
        
        **add_component** - Add new widget component with validation
        ```python
        manage_umg_widget(
            action="add_component",
            widget_name="WBP_MainMenu",
            component_type="Button",
            component_name="PlayButton",
            parent_name="CanvasPanel_Root",
            properties={"Text": "Play Game", "ColorAndOpacity": [0, 1, 0, 1]}
        )
        ```
        
        **remove_component** - Remove component from widget hierarchy
        ```python
        manage_umg_widget(
            action="remove_component",
            widget_name="WBP_Inventory",
            component_name="CloseButton",
            remove_children=True,
            remove_from_variables=True
        )
        ```
        
        **validate** - Validate widget hierarchy and structure
        ```python
        manage_umg_widget(
            action="validate",
            widget_name="WBP_Inventory"
        )
        ```
        
        ### Type Discovery Actions
        
        **search_types** - Discover available widget component types
        ```python
        manage_umg_widget(
            action="search_types",
            category="Common",
            search_text="Button",
            include_custom=True
        )
        ```
        
        **get_component_properties** - Get all properties of specific component
        ```python
        manage_umg_widget(
            action="get_component_properties",
            widget_name="WBP_Inventory",
            component_name="TitleText"
        )
        ```
        
        ### Property Management Actions
        
        **get_property** - Get single property value
        ```python
        manage_umg_widget(
            action="get_property",
            widget_name="WBP_Inventory",
            component_name="TitleText",
            property_name="Text"
        )
        ```
        
        **set_property** - Set single property value (including slot properties)
        ```python
        # Set text color
        manage_umg_widget(
            action="set_property",
            widget_name="WBP_Inventory",
            component_name="TitleText",
            property_name="ColorAndOpacity",
            property_value=[0.0, 1.0, 1.0, 1.0]
        )
        
        # Set slot property for background fill
        manage_umg_widget(
            action="set_property",
            widget_name="WBP_Inventory",
            component_name="Background",
            property_name="Slot.HorizontalAlignment",
            property_value="HAlign_Fill"
        )
        ```
        
        **list_properties** - List all available properties for component
        ```python
        manage_umg_widget(
            action="list_properties",
            widget_name="WBP_Inventory",
            component_name="TitleText",
            include_inherited=True,
            category_filter="Appearance"
        )
        ```
        
        ### Event Management Actions
        
        **get_available_events** - Get available events for component
        ```python
        manage_umg_widget(
            action="get_available_events",
            widget_name="WBP_MainMenu",
            component_name="PlayButton"
        )
        ```
        
        **bind_events** - Bind multiple input events at once
        ```python
        manage_umg_widget(
            action="bind_events",
            widget_name="WBP_MainMenu",
            component_name="PlayButton",
            input_events={
                "OnClicked": "HandlePlayClicked",
                "OnHovered": "HandlePlayHovered"
            }
        )
        ```
        
        ##  Parameters by Action:
        
        **Component Lifecycle:**
        - list_components: widget_name
        - add_component: widget_name, component_type, component_name, parent_name (opt), is_variable (opt), properties (opt)
        - remove_component: widget_name, component_name, remove_children (opt), remove_from_variables (opt)
        - validate: widget_name
        
        **Type Discovery:**
        - search_types: category (opt), search_text (opt), include_custom (opt), include_engine (opt), parent_compatibility (opt)
        - get_component_properties: widget_name, component_name
        
        **Property Management:**
        - get_property: widget_name, component_name, property_name
        - set_property: widget_name, component_name, property_name, property_value, property_type (opt)
        - list_properties: widget_name, component_name, include_inherited (opt), category_filter (opt)
        
        **Event Management:**
        - get_available_events: widget_name, component_name
        - bind_events: widget_name, component_name, input_events
        
        ##  Common Workflows:
        
        **Discovery → Inspect → Modify:**
        ```python
        # 1. List components to find targets
        manage_umg_widget(action="list_components", widget_name="WBP_Inventory")
        
        # 2. Get current properties
        manage_umg_widget(action="get_component_properties", widget_name="WBP_Inventory", component_name="TitleText")
        
        # 3. Set new property values
        manage_umg_widget(action="set_property", widget_name="WBP_Inventory", component_name="TitleText",
                         property_name="ColorAndOpacity", property_value=[0, 1, 1, 1])
        ```
        
        **Widget Type Discovery → Component Creation:**
        ```python
        # 1. Find available widget types
        manage_umg_widget(action="search_types", category="Common", search_text="Button")
        
        # 2. Add component
        manage_umg_widget(action="add_component", widget_name="WBP_MainMenu", component_type="Button",
                         component_name="PlayButton", parent_name="CanvasPanel_Root")
        ```
        
        ## Critical Notes:
        
        **Slot Properties (Background Filling):**
        - Use "Slot.HorizontalAlignment" with values: "HAlign_Fill", "HAlign_Left", "HAlign_Center", "HAlign_Right"
        - Use "Slot.VerticalAlignment" with values: "VAlign_Fill", "VAlign_Top", "VAlign_Center", "VAlign_Bottom"
        - Use "Slot.Size.SizeRule" with values: "Fill", "Auto"
        
        **Property Value Formats:**
        - Colors: [R, G, B, A] with 0.0-1.0 values, e.g., [0.0, 1.0, 1.0, 1.0]
        - Fonts: {"Size": number, "TypefaceFontName": string}, e.g., {"Size": 24, "TypefaceFontName": "Bold"}
        - Padding: {"Left": n, "Top": n, "Right": n, "Bottom": n}
        - Visibility: "Visible", "Hidden", "Collapsed", "HitTestInvisible"
        
        Action Mappings:
        - list_widget_components() → action="list_components"
        - add_widget_component() → action="add_component"
        - remove_umg_component() → action="remove_component"
        - validate_widget_hierarchy() → action="validate"
        - get_available_widgets() / get_available_widget_types() → action="search_types"
        - get_widget_component_properties() → action="get_component_properties"
        - get_widget_property() → action="get_property"
        - set_widget_property() → action="set_property"
        - list_widget_properties() → action="list_properties"
        - get_available_events() → action="get_available_events"
        - bind_input_events() → action="bind_events"
        
        Args:
            action: Action to perform (see above for available actions)
            widget_name: Target Widget Blueprint name (required for most actions)
            component_name: Target component name (for component-specific actions)
            component_type: Widget class name (for add_component)
            parent_name: Parent component name (for add_component, default: "root")
            property_name: Property name (for get/set property actions)
            property_value: Value to set (for set_property action)
            property_type: Type hint for complex properties (default: "auto")
            is_variable: Make component a Blueprint variable (default: True)
            properties: Initial properties dict (for add_component)
            remove_children: Remove child components (for remove_component, default: True)
            remove_from_variables: Remove Blueprint variable (for remove_component, default: True)
            category: Filter by category (for search_types)
            search_text: Text search filter (for search_types)
            include_custom: Include custom widgets (for search_types, default: True)
            include_engine: Include engine widgets (for search_types, default: True)
            parent_compatibility: Filter by parent type (for search_types)
            include_inherited: Include inherited properties (for list_properties, default: True)
            category_filter: Filter by property category (for list_properties)
            input_events: Event mapping dict (for bind_events)
            options: Additional action-specific options
            
        Returns:
            Dict containing action results with success field and action-specific data
        """
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
            
            if action not in valid_actions:
                return {
                    "success": False,
                    "error": f"Invalid action '{action}'. Valid actions: {', '.join(valid_actions)}"
                }
            
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
                return {
                    "success": False,
                    "error": f"Action '{action}' not implemented yet"
                }
                
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
        return {"success": False, "error": "widget_name is required for list_components action"}
    
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
