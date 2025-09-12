"""
UMG Widget Property Management and Styling Tools

ADVANCED STYLING SYSTEM FOR AI ASSISTANTS

⚠️ **CRITICAL FOR AI**: Before using any styling or background tools, ALWAYS call 
get_umg_guide() first to understand proper widget hierarchy, container-specific 
background implementation patterns, and styling best practices.

This module provides the enhanced UMG styling system with:
- Universal property setting with complex type support
- Style set creation and management for consistent theming
- Transform and layout property management
- Visibility and Z-order control
- Slot property management for layout containers

KEY FEATURES FOR AI UNDERSTANDING:
1. Style Sets: Create reusable themes that can be applied to multiple components
2. Complex Properties: Support for Color, Font, Padding objects with nested properties
3. Transform System: Comprehensive positioning, sizing, scaling, and anchoring
4. Slot Management: Layout-specific properties for different container types

REQUIRED AI WORKFLOW:
1. ALWAYS call get_umg_guide() FIRST before styling widgets
2. Use search_items() to find target widgets
3. Use list_widget_components() to understand current structure
4. Apply styling following container-specific patterns from guide

USAGE PATTERNS:
- Use set_widget_property() for individual property changes
- Use create_widget_style_set() + apply_widget_theme() for consistent theming
- Use set_widget_transform() for comprehensive layout changes

PROPERTY TYPE SUPPORT:
- Simple: strings, numbers, booleans
- Colors: [R,G,B,A] arrays or {"R":value, "G":value, "B":value, "A":value} objects  
- Fonts: {"Size": number, "TypefaceFontName": string} objects
- Padding/Margin: {"Left":n, "Top":n, "Right":n, "Bottom":n} objects
- Transforms: Position, Size, Scale arrays [X,Y] or [X,Y,Z]
"""

import logging
from typing import Dict, Any, List, Optional, Union
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_umg_styling_tools(mcp: FastMCP):
    """
    Register UMG property management and styling tools with the MCP server.
    
    ⚠️ **AI INTEGRATION NOTE**: These tools require the UMG guide for proper usage. 
    AI assistants should call get_umg_guide() before using styling tools to understand 
    container-specific patterns and widget hierarchy requirements.
    """
    
    # Universal Property Management
    @mcp.tool()
    def set_widget_property(
        ctx: Context,
        widget_name: str,
        component_name: str,
        property_name: str,
        property_value: Any,
        property_type: str = "auto"
    ) -> Dict[str, Any]:
        """
        Set any property on any widget component.
        
        ⚠️ **IMPORTANT**: Before styling widgets, ALWAYS use get_umg_guide() first to understand 
        proper container-specific background implementation, widget hierarchy requirements, 
        and styling best practices. This ensures correct implementation patterns.
        
        🎨 **UNIVERSAL PROPERTY SETTER**: Use this for all widget property modifications.
        Supports simple values, complex objects, and automatic type detection.
        
        Args:
            widget_name: Name of the target Widget Blueprint
                        ⚠️ **Must be exact name from search_items() results**
            component_name: Name of the target component
                        ⚠️ **Must be exact name from list_widget_components() results**
            property_name: Property to set
                        Examples: "Text", "Visibility", "IsEnabled", "ColorAndOpacity", "Font"
            property_value: Value to set (auto-detects type)
                        - Strings: "Hello World", "Visible", "Collapsed"
                        - Numbers: 42, 3.14, 255
                        - Booleans: true, false
                        - Colors: [1.0, 0.0, 0.5, 1.0] or {"R": 1.0, "G": 0.0, "B": 0.5, "A": 1.0}
                        - Arrays: [100, 200] for positions/sizes
            property_type: Type hint for complex properties (usually auto-detected)
                        Examples: "Color", "Font", "Padding", "Vector2D"
            
        Returns:
            Dict containing:
            - success: boolean indicating if property was set
            - property_name: name of property that was set
            - component_name: name of component that was modified
            - error: string (only if success=false)
            
        📝 **Sci-Fi Styling Examples**:
        ```python
        # Neon cyan text
        set_widget_property("WBP_Inventory", "Title_Text", "ColorAndOpacity", [0.0, 1.0, 1.0, 1.0])
        
        # Dark background with transparency
        set_widget_property("WBP_Inventory", "Background_Image", "ColorAndOpacity", [0.1, 0.0, 0.2, 0.8])
        
        # Glowing border color
        set_widget_property("WBP_Inventory", "Border_Panel", "BrushColor", [0.0, 1.0, 0.5, 1.0])
        
        # Large tech-style font
        set_widget_property("WBP_Inventory", "Header_Text", "Font", {"Size": 24, "TypefaceFontName": "Bold"})
        
        # Button hover glow
        set_widget_property("WBP_Inventory", "Close_Button", "ColorAndOpacity", [1.0, 0.0, 1.0, 1.0])
        ```
        
        🚨 **AI Best Practices**:
        1. Use list_widget_components() first to get exact component names
        2. For colors, use [R,G,B,A] format with 0.0-1.0 values
        3. For text, check if property is "Text" or "Content"  
        4. For visibility, use "Visible", "Hidden", "Collapsed", "HitTestInvisible"
        5. For booleans, use lowercase true/false
        
        💡 **Common Properties**:
        - **Text Components**: "Text", "ColorAndOpacity", "Font", "Justification"
        - **Image Components**: "ColorAndOpacity", "BrushColor", "Brush"
        - **Button Components**: "Style", "ColorAndOpacity", "BackgroundColor"
        - **Panel Components**: "BrushColor", "Padding", "BackgroundColor"
        - **All Components**: "Visibility", "IsEnabled", "ToolTipText"
        """
        from vibe_ue_server import get_unreal_connection
        
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
            
            logger.info(f"Setting property '{property_name}' on '{component_name}' to '{property_value}'")
            response = unreal.send_command("set_widget_property", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Set widget property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting widget property: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def get_widget_property(
        ctx: Context,
        widget_name: str,
        component_name: str,
        property_name: str
    ) -> Dict[str, Any]:
        """
        Get current value of a widget property.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            component_name: Name of the target component
            property_name: Property to get
            
        Returns:
            Dict containing property value and metadata
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine", "property_value": None}
            
            params = {
                "widget_name": widget_name,
                "component_name": component_name,
                "property_name": property_name
            }
            
            logger.info(f"Getting property '{property_name}' from '{component_name}'")
            response = unreal.send_command("get_widget_property", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine", "property_value": None}
            
            logger.info(f"Get widget property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting widget property: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg, "property_value": None}

    @mcp.tool()
    def list_widget_properties(
        ctx: Context,
        widget_name: str,
        component_name: str,
        include_inherited: bool = True,
        category_filter: str = ""
    ) -> Dict[str, Any]:
        """
        List all available properties for a widget component.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            component_name: Name of the target component
            include_inherited: Whether to include inherited properties
            category_filter: Filter properties by category (optional)
            
        Returns:
            Dict containing list of available properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine", "properties": []}
            
            params = {
                "widget_name": widget_name,
                "component_name": component_name,
                "include_inherited": include_inherited,
                "category_filter": category_filter
            }
            
            logger.info(f"Listing properties for '{component_name}'")
            response = unreal.send_command("list_widget_properties", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine", "properties": []}
            
            logger.info(f"List widget properties response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error listing widget properties: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg, "properties": []}

    # Advanced Styling Tools
    @mcp.tool()
    def set_widget_transform(
        ctx: Context,
        widget_name: str,
        component_name: str,
        position: Optional[List[float]] = None,
        size: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        rotation: Optional[float] = None,
        anchor_min: Optional[List[float]] = None,
        anchor_max: Optional[List[float]] = None,
        alignment: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """
        Set comprehensive transform and layout properties.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            component_name: Name of the target component
            position: [X, Y] position
            size: [Width, Height] size
            scale: [X, Y] scale values
            rotation: Rotation angle in degrees
            anchor_min: [X, Y] minimum anchor point (0.0 to 1.0)
            anchor_max: [X, Y] maximum anchor point (0.0 to 1.0)
            alignment: [X, Y] alignment values (0.0 to 1.0)
            
        Returns:
            Dict containing success status
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "component_name": component_name,
                "position": position,
                "size": size,
                "scale": scale,
                "rotation": rotation,
                "anchor_min": anchor_min,
                "anchor_max": anchor_max,
                "alignment": alignment
            }
            
            logger.info(f"Setting transform properties for '{component_name}'")
            response = unreal.send_command("set_widget_transform", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Set widget transform response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting widget transform: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def set_widget_visibility(
        ctx: Context,
        widget_name: str,
        component_name: str,
        visibility: str = "Visible"
    ) -> Dict[str, Any]:
        """
        Set widget visibility state.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            component_name: Name of the target component
            visibility: Visibility state ("Visible", "Hidden", "Collapsed", "HitTestInvisible", "SelfHitTestInvisible")
            
        Returns:
            Dict containing success status
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "component_name": component_name,
                "visibility": visibility
            }
            
            logger.info(f"Setting visibility of '{component_name}' to '{visibility}'")
            response = unreal.send_command("set_widget_visibility", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Set widget visibility response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting widget visibility: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def set_widget_z_order(
        ctx: Context,
        widget_name: str,
        component_name: str,
        z_order: int
    ) -> Dict[str, Any]:
        """
        Set widget z-order for layering.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            component_name: Name of the target component
            z_order: Z-order value (higher values appear on top)
            
        Returns:
            Dict containing success status
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "component_name": component_name,
                "z_order": z_order
            }
            
            logger.info(f"Setting z-order of '{component_name}' to {z_order}")
            response = unreal.send_command("set_widget_z_order", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Set widget z-order response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting widget z-order: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    logger.info("UMG styling tools registered successfully")
