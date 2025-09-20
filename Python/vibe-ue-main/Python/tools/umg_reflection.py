"""
UMG Widget Reflection System

GENERIC WIDGET CREATION SYSTEM FOR AI ASSISTANTS

This module provides a reflection-based widget system that mirrors Unreal's Widget Palette:
- Automatic widget class discovery using UClass reflection
- Category-based widget filtering and organization  
- Parent-child compatibility valid        except Exception as e:
            logger.error(f"Error adding widget component: {str(e)}")
            return {
                "success": False,
                "error": f"Component creation failed: {str(e)}",
                "component_name": component_name,
                "component_type": component_type
            }eneric widget component creation with property support
- Type-safe parameter validation and error handling

CRITICAL FOR AI WORKFLOW:
1. Use get_available_widgets() to discover all widget types with metadata
2. Use category filtering to find appropriate widget types for your use case
3. Use parent_compatibility to ensure widgets can be added to containers
4. Use add_widget_component() for generic widget creation with validation

REFLECTION ADVANTAGES:
- Future-proof: Automatically discovers new widget types
- Consistent: Uses same validation as Unreal's Widget Palette
- Complete: Includes both engine and custom widget types
- Intelligent: Provides compatibility and constraint information

USAGE PATTERNS:
- get_available_widgets() - Discover all widget types
- get_available_widgets(category="Panel") - Find all panel widgets
- get_available_widgets(parent_compatibility="CanvasPanel") - Find widgets compatible with CanvasPanel
- add_widget_component() - Create any widget type with validation

CATEGORY SYSTEM:
- "Panel": Container widgets (CanvasPanel, VerticalBox, etc.)
- "Common": Interactive widgets (Button, etc.)
- "Input": Input widgets (EditableText, Slider, CheckBox, etc.)
- "Display": Display widgets (TextBlock, Image, ProgressBar, etc.)  
- "Primitive": Basic widgets (Spacer, Border, etc.)
- "User Created": Custom widget blueprints

VALIDATION FEATURES:
- Parent-child compatibility checking
- Child count constraint validation
- Widget type existence verification
- Property type validation and conversion
"""

import logging
from typing import Dict, Any, List, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_umg_reflection_tools(mcp: FastMCP):
    """Register UMG reflection-based widget tools with the MCP server."""
    
    @mcp.tool()
    def get_available_widgets(
        ctx: Context,
        category: str = "",
        include_custom: bool = True,
        include_engine: bool = True,
        parent_compatibility: str = ""
    ) -> Dict[str, Any]:
        """
        Get all available widget types using Unreal's reflection system.
        
        🔍 **WIDGET PALETTE DISCOVERY**: Discover all widget types available in Unreal
        using the same reflection system as the Widget Palette, with filtering and metadata.
        
        Args:
            category: Filter by widget category (optional)
                     Examples: "Panel", "Common", "Input", "Display", "Primitive", "User Created"
            include_custom: Whether to include custom widget blueprints (default: True)
            include_engine: Whether to include engine widget types (default: True)  
            parent_compatibility: Filter widgets compatible with specified parent type (optional)
                                  Example: "CanvasPanel", "VerticalBox", "ContentWidget"
            
        Returns:
            Dict containing:
            - success: boolean indicating if discovery completed
            - widgets: array of widget type objects with metadata
            - categories: array of all available categories
            - count: total number of widget types found
            - error: string (only if success=false)
            
        📊 **Widget Metadata Structure**:
        ```json
        {
            "name": "Button",
            "display_name": "Button",
            "category": "Common",
            "class_path": "/Script/UMG.Button",
            "is_custom": false,
            "supports_children": true,
            "max_children": 1,
            "supported_child_types": ["*"],
            "description": "Interactive button widget"
        }
        ```
        
        📝 **Usage Examples**:
        ```python
        # Get all available widgets
        get_available_widgets()
        
        # Get only panel widgets for containers
        get_available_widgets(category="Panel")
        
        # Get widgets that can be added to a CanvasPanel
        get_available_widgets(parent_compatibility="CanvasPanel")
        
        # Get only engine widgets (no custom blueprints)
        get_available_widgets(include_custom=False)
        
        # Get only input widgets
        get_available_widgets(category="Input")
        ```
        
        🎯 **AI Integration Tips**:
        - Use category filtering to find appropriate widget types
        - Check supports_children before trying to add child widgets
        - Use parent_compatibility to ensure valid widget placement
        - Custom widgets appear in "User Created" category
        """
        
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {
                    "success": False,
                    "error": "Failed to connect to Unreal Engine. Ensure Unreal is running with VibeUE plugin loaded.",
                    "widgets": [],
                    "categories": [],
                    "count": 0
                }
            
            command_data = {
                "category": category,
                "include_custom": include_custom,
                "include_engine": include_engine,
                "parent_compatibility": parent_compatibility
            }
            
            logger.info(f"Getting available widgets with filters: category={category}, include_custom={include_custom}, include_engine={include_engine}")
            response = unreal.send_command("get_available_widgets", command_data)
            
            logger.debug(f"Raw response from get_available_widgets: {response}")
            
            if not response.get("success", False):
                error_msg = response.get("error", "Unknown error during widget discovery")
                logger.error(f"Widget discovery failed: {error_msg}")
                logger.error(f"Full response: {response}")
                
                # Check if we got a completely empty response
                if not response:
                    error_msg = "No response received from Unreal Engine. Command may not be implemented or plugin not loaded."
                elif error_msg == "Unknown error during widget discovery":
                    # This means C++ returned success=false with no error message
                    error_msg = "C++ command returned failure with no error details. Check Unreal Engine Output Log for C++ errors or exceptions."
                    if "command" in response.get("error", "").lower():
                        error_msg += " The get_available_widgets command may not be properly registered in Bridge.cpp."
                
                # Provide more specific error messages based on common issues
                if "not found" in error_msg.lower():
                    error_msg = f"Command handler not found: {error_msg}. Ensure VibeUE plugin is loaded and UMGReflectionCommands are registered."
                elif "connection" in error_msg.lower():
                    error_msg = f"Connection issue: {error_msg}. Check if Unreal Engine is running and MCP server is connected."
                elif "exception" in error_msg.lower():
                    error_msg = f"C++ exception occurred: {error_msg}. Check Unreal Engine Output Log for details."
                
                return {
                    "success": False,
                    "error": error_msg,
                    "widgets": [],
                    "categories": [],
                    "count": 0,
                    "debug_info": {
                        "raw_response": response,
                        "command_sent": "get_available_widgets",
                        "parameters": command_data,
                        "suggestion": "Check Unreal Engine Output Log window for C++ error messages"
                    }
                }
            
            widgets = response.get("widgets", [])
            categories = response.get("categories", [])
            
            logger.info(f"Discovered {len(widgets)} widget types across {len(categories)} categories")
            
            return {
                "success": True,
                "widgets": widgets,
                "categories": categories,
                "count": len(widgets),
                "discovery_info": {
                    "category_filter": category if category else "all",
                    "include_custom": include_custom,
                    "include_engine": include_engine,
                    "parent_compatibility": parent_compatibility if parent_compatibility else "any"
                }
            }
            
        except Exception as e:
            logger.error(f"Error during widget discovery: {str(e)}")
            logger.error(f"Exception type: {type(e).__name__}")
            logger.error(f"Command data: {command_data}")
            
            # Check if it's a connection issue
            if "connection" in str(e).lower() or "timeout" in str(e).lower():
                error_msg = f"Connection failed: {str(e)}. Ensure Unreal Engine is running and VibeUE plugin is loaded."
            elif "command" in str(e).lower():
                error_msg = f"Command execution failed: {str(e)}. Check if get_available_widgets is implemented in C++."
            else:
                error_msg = f"Unexpected error during widget discovery: {str(e)}"
            
            return {
                "success": False,
                "error": error_msg,
                "widgets": [],
                "categories": [],
                "count": 0,
                "debug_info": {
                    "exception_type": type(e).__name__,
                    "exception_message": str(e),
                    "command_sent": "get_available_widgets",
                    "parameters": command_data
                }
            }
    
    @mcp.tool()
    def add_widget_component(
        ctx: Context,
        widget_name: str,
        component_type: str,
        component_name: str,
        parent_name: str = "root",
        is_variable: bool = True,
        properties: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """
        Add any type of widget component using generic reflection-based creation.
        
        🎯 **UNIVERSAL WIDGET CREATOR**: Create any widget type using Unreal's reflection
        system with automatic validation and compatibility checking.
        
        Args:
            widget_name: Name of the target Widget Blueprint
                        ⚠️ **Must be exact name from search_items() results**
            component_type: Widget class name to create
                           Examples: "Button", "TextBlock", "CanvasPanel", "VerticalBox", "Image"
                           Use get_available_widgets() to see all available types
            component_name: Name for the new widget component
                           Examples: "MainButton", "TitleText", "BackgroundPanel"
            parent_name: Name of parent component to add widget to (default: "root")
                        Examples: "CanvasPanel_Main", "VerticalBox_Content", "root"
            is_variable: Whether widget should be a variable accessible in Blueprint (default: True)
            properties: Initial properties to set on the widget (optional)
                       Examples: {"Text": "Click Me", "Visibility": "Visible"}
            
        Returns:
            Dict containing:
            - success: boolean indicating if widget was created
            - component_name: name of created component
            - component_type: type of widget that was created
            - widget_name: name of widget blueprint modified
            - parent_name: name of parent component
            - validation: validation results and compatibility info
            - error: string (only if success=false)
            
        📝 **Usage Examples**:
        ```python
        # Add a button to canvas panel
        add_widget_component("WBP_MainMenu", "Button", "PlayButton", "CanvasPanel_Root")
        
        # Add text with initial properties
        add_widget_component("WBP_HUD", "TextBlock", "HealthText", "StatusPanel", 
                            properties={"Text": "100%", "ColorAndOpacity": [0, 1, 0, 1]})
        
        # Add vertical box as layout container
        add_widget_component("WBP_Inventory", "VerticalBox", "ItemList", "ScrollBox_Items")
        
        # Add image with texture
        add_widget_component("WBP_UI", "Image", "Background", "root",
                            properties={"Brush.Texture": "/Game/Textures/UI_Background"})
        
        # Add slider with range settings
        add_widget_component("WBP_Settings", "Slider", "VolumeSlider", "OptionsPanel",
                            properties={"MinValue": 0.0, "MaxValue": 1.0, "Value": 0.8})
        ```
        
        ⚠️ **Validation Features**:
        - Automatic parent-child compatibility checking
        - Widget type existence verification  
        - Child count constraint validation
        - Property type validation and conversion
        - Blueprint compilation integration
        
        🎨 **Property Examples**:
        ```python
        # Text properties
        {"Text": "Hello World", "ColorAndOpacity": [1, 1, 1, 1]}
        
        # Image properties
        {"Brush.Texture": "/Game/Textures/Icon", "ColorAndOpacity": [1, 0, 0, 1]}
        
        # Layout properties
        {"Padding": [10, 5, 10, 5], "HorizontalAlignment": "HAlign_Center"}
        
        # Visibility and interaction
        {"Visibility": "Visible", "IsEnabled": True}
        ```
        
        🚨 **AI Best Practices**:
        1. Use get_available_widgets() first to discover valid component types
        2. Use exact component names from get_available_widgets() results
        3. Verify parent exists using list_widget_components() before adding
        4. Use descriptive component names for Blueprint organization
        5. Set is_variable=True for components you'll interact with in code
        """
        
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {
                    "success": False,
                    "error": "Failed to connect to Unreal Engine. Ensure Unreal is running with VibeUE plugin loaded.",
                    "component_name": component_name,
                    "component_type": component_type
                }
            
            # Prepare properties for serialization
            properties_dict = properties if properties else {}
            
            command_data = {
                "widget_name": widget_name,
                "component_type": component_type,
                "component_name": component_name,
                "parent_name": parent_name,
                "is_variable": is_variable,
                "properties": properties_dict
            }
            
            logger.info(f"Adding widget component '{component_name}' of type '{component_type}' to widget '{widget_name}'")
            response = unreal.send_command("add_widget_component", command_data)
            
            # DEBUG: Log the actual response structure
            logger.info(f"DEBUG: Received response from add_widget_component: {response}")
            logger.info(f"DEBUG: Response type: {type(response)}")
            if response:
                logger.info(f"DEBUG: Response keys: {response.keys() if hasattr(response, 'keys') else 'No keys method'}")
                logger.info(f"DEBUG: Success value: {response.get('success')} (type: {type(response.get('success'))})")
            
            if not response.get("success", False):
                # More detailed error information
                error_details = {
                    "raw_response": response,
                    "success_value": response.get("success"),
                    "success_type": str(type(response.get("success"))),
                    "response_keys": list(response.keys()) if hasattr(response, 'keys') else None,
                    "response_type": str(type(response))
                }
                return {
                    "success": False,
                    "error": "DEBUGGING: This error message should show if changes are taking effect. Response was: " + str(response),
                    "component_name": component_name,
                    "component_type": component_type
                }
            
            logger.info(f"Successfully added {component_type} '{component_name}' to widget '{widget_name}'")
            
            return {
                "success": True,
                "component_name": response.get("component_name", component_name),
                "component_type": response.get("component_type", component_type),
                "widget_name": response.get("widget_name", widget_name),
                "parent_name": response.get("parent_name", parent_name),
                "validation": response.get("validation", {}),
                "creation_info": {
                    "is_variable": is_variable,
                    "initial_properties_applied": len(properties_dict),
                    "widget_modified": True
                }
            }
            
        except Exception as e:
            logger.error(f"Error adding widget component: {str(e)}")
            return {
                "success": False,
                "error": f"Widget component creation failed: {str(e)}",
                "component_name": component_name,
                "component_type": component_type
            }