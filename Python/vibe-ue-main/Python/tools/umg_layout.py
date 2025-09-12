"""
UMG Widget Layout Management Tools

⚠️ **CRITICAL FOR AI**: Before using layout tools, especially for backgrounds and overlays, 
ALWAYS call get_umg_guide() first to understand proper container-specific implementation 
patterns, Canvas Panel background requirements, and widget hierarchy rules.

This module provides tools for managing widget layout containers including
canvas panels, boxes, grids, scroll areas, and hierarchical relationships.
"""

import logging
from typing import Dict, Any, List, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_umg_layout_tools(mcp: FastMCP):
    """
    Register UMG layout management tools with the MCP server.
    
    ⚠️ **AI INTEGRATION NOTE**: Layout tools (especially overlays and Canvas Panels) 
    require specific implementation patterns. AI assistants should call get_umg_guide() 
    first to understand proper background implementation and container requirements.
    """
    
    # Layout Container Tools
    @mcp.tool()
    def add_canvas_panel(
        ctx: Context,
        widget_name: str,
        panel_name: str,
        parent_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [400.0, 300.0]
    ) -> Dict[str, Any]:
        """
        Add a Canvas Panel for absolute positioning.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            panel_name: Name for the new Canvas Panel
            parent_name: Name of the parent container to add the panel to
            position: [X, Y] position of the panel
            size: [Width, Height] of the panel
            
        Returns:
            Dict containing success status and panel properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "panel_name": panel_name,
                "parent_name": parent_name,
                "position": position,
                "size": size
            }
            
            logger.info(f"Adding Canvas Panel '{panel_name}' to parent '{parent_name}' in widget '{widget_name}'")
            response = unreal.send_command("add_canvas_panel", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add canvas panel response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding canvas panel: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_overlay(
        ctx: Context,
        widget_name: str,
        overlay_name: str,
        parent_name: str
    ) -> Dict[str, Any]:
        """
        Add an Overlay widget for layered content (required for Canvas Panel backgrounds).
        
        ⚠️ **IMPORTANT**: Before adding overlays for backgrounds, ALWAYS use get_umg_guide() 
        first to understand proper Canvas Panel background implementation patterns and 
        container-specific requirements.
        
        Overlay widgets provide Z-ordered layering where the first child renders as the background
        and subsequent children render on top. This is essential for adding backgrounds to Canvas Panels.
        
        IMPORTANT: You MUST specify a parent_name parameter! This should be the name of an existing 
        panel component in the widget (e.g., "CanvasPanel_Body", "RootPanel", etc.). Use the 
        list_widget_components function first to see available parent containers.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            overlay_name: Name for the new Overlay widget  
            parent_name: REQUIRED - Name of existing parent container (use list_widget_components to find valid names)
            
        Examples:
            - parent_name: "CanvasPanel_Body" (common canvas panel name)
            - parent_name: "RootPanel" (typical root container)
            - parent_name: "MainContainer" (custom container name)
            
        Returns:
            Dict containing success status and overlay properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "overlay_name": overlay_name,
                "parent_name": parent_name
            }
            
            logger.info(f"Adding Overlay '{overlay_name}' to parent '{parent_name}' in widget '{widget_name}'")
            response = unreal.send_command("add_overlay", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add overlay response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding overlay: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}


    @mcp.tool()
    def add_horizontal_box(
        ctx: Context,
        widget_name: str,
        box_name: str,
        parent_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [400.0, 50.0],
        padding: List[float] = [0.0, 0.0, 0.0, 0.0]
    ) -> Dict[str, Any]:
        """
        Add a Horizontal Box for linear horizontal layout.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            box_name: Name for the new Horizontal Box
            parent_name: Name of the parent container to add the box to
            position: [X, Y] position of the box
            size: [Width, Height] of the box
            padding: [Left, Top, Right, Bottom] padding values
            
        Returns:
            Dict containing success status and box properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "box_name": box_name,
                "parent_name": parent_name,
                "position": position,
                "size": size,
                "padding": padding
            }
            
            logger.info(f"Adding Horizontal Box '{box_name}' to parent '{parent_name}' in widget '{widget_name}'")
            response = unreal.send_command("add_horizontal_box", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add horizontal box response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding horizontal box: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_vertical_box(
        ctx: Context,
        widget_name: str,
        box_name: str,
        parent_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 300.0],
        padding: List[float] = [0.0, 0.0, 0.0, 0.0]
    ) -> Dict[str, Any]:
        """
        Add a Vertical Box for linear vertical layout.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            box_name: Name for the new Vertical Box
            parent_name: Name of the parent container to add the box to
            position: [X, Y] position of the box
            size: [Width, Height] of the box
            padding: [Left, Top, Right, Bottom] padding values
            
        Returns:
            Dict containing success status and box properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "box_name": box_name,
                "parent_name": parent_name,
                "position": position,
                "size": size,
                "padding": padding
            }
            
            logger.info(f"Adding Vertical Box '{box_name}' to parent '{parent_name}' in widget '{widget_name}'")
            response = unreal.send_command("add_vertical_box", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add vertical box response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding vertical box: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_scroll_box(
        ctx: Context,
        widget_name: str,
        scroll_box_name: str,
        parent_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 200.0],
        orientation: str = "Vertical",
        scroll_bar_visibility: str = "Visible",
        always_show_scrollbar: bool = False
    ) -> Dict[str, Any]:
        """
        Add a Scroll Box for scrollable content.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            scroll_box_name: Name for the new Scroll Box
            parent_name: Name of the parent container to add the scroll box to
            position: [X, Y] position of the scroll box
            size: [Width, Height] of the scroll box
            orientation: Scroll orientation ("Vertical" or "Horizontal")
            scroll_bar_visibility: Scrollbar visibility ("Visible", "Collapsed", "Hidden")
            always_show_scrollbar: Whether to always show scrollbar
            
        Returns:
            Dict containing success status and scroll box properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "scroll_box_name": scroll_box_name,
                "parent_name": parent_name,
                "position": position,
                "size": size,
                "orientation": orientation,
                "scroll_bar_visibility": scroll_bar_visibility,
                "always_show_scrollbar": always_show_scrollbar
            }
            
            logger.info(f"Adding Scroll Box '{scroll_box_name}' to parent '{parent_name}' in widget '{widget_name}'")
            response = unreal.send_command("add_scroll_box", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add scroll box response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding scroll box: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_grid_panel(
        ctx: Context,
        widget_name: str,
        grid_panel_name: str,
        parent_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [400.0, 300.0],
        columns: int = 2,
        rows: int = 2
    ) -> Dict[str, Any]:
        """
        Add a Grid Panel for tabular layout.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            grid_panel_name: Name for the new Grid Panel
            parent_name: Name of the parent container to add the grid to
            position: [X, Y] position of the grid
            size: [Width, Height] of the grid
            columns: Number of columns
            rows: Number of rows
            
        Returns:
            Dict containing success status and grid properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "grid_panel_name": grid_panel_name,
                "parent_name": parent_name,
                "position": position,
                "size": size,
                "columns": columns,
                "rows": rows
            }
            
            logger.info(f"Adding Grid Panel '{grid_panel_name}' to parent '{parent_name}' in widget '{widget_name}'")
            response = unreal.send_command("add_grid_panel", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add grid panel response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding grid panel: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    # Hierarchical Management Tools
    @mcp.tool()
    def add_child_to_panel(
        ctx: Context,
        widget_name: str,
        parent_panel_name: str,
        child_widget_name: str,
        slot_index: int = -1,
        position: Optional[List[float]] = None,
        size: Optional[List[float]] = None,
        anchors: Optional[Dict[str, float]] = None,
        alignment: List[float] = [0.0, 0.0]
    ) -> Dict[str, Any]:
        """
        Add a child widget to a parent panel with layout constraints.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            parent_panel_name: Name of the parent panel
            child_widget_name: Name of the child widget to add
            slot_index: Index position in parent (-1 for end)
            position: [X, Y] position for Canvas Panel children
            size: [Width, Height] size for Canvas Panel children
            anchors: Anchor configuration for Canvas Panel children
            alignment: [X, Y] alignment values (0.0 to 1.0)
            
        Returns:
            Dict containing success status and child placement info
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "parent_panel_name": parent_panel_name,
                "child_widget_name": child_widget_name,
                "slot_index": slot_index,
                "position": position,
                "size": size,
                "anchors": anchors,
                "alignment": alignment
            }
            
            logger.info(f"Adding child '{child_widget_name}' to parent '{parent_panel_name}'")
            response = unreal.send_command("add_child_to_panel", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add child to panel response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding child to panel: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def remove_child_from_panel(
        ctx: Context,
        widget_name: str,
        parent_panel_name: str,
        child_widget_name: str
    ) -> Dict[str, Any]:
        """
        Remove a child widget from a parent panel.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            parent_panel_name: Name of the parent panel
            child_widget_name: Name of the child widget to remove
            
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
                "parent_panel_name": parent_panel_name,
                "child_widget_name": child_widget_name
            }
            
            logger.info(f"Removing child '{child_widget_name}' from parent '{parent_panel_name}'")
            response = unreal.send_command("remove_child_from_panel", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Remove child from panel response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error removing child from panel: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def set_widget_slot_properties(
        ctx: Context,
        widget_name: str,
        widget_component_name: str,
        slot_properties: Dict[str, Any]
    ) -> Dict[str, Any]:
        """
        Set slot-specific properties for a widget (padding, alignment, etc.).
        
        Args:
            widget_name: Name of the target Widget Blueprint
            widget_component_name: Name of the widget component
            slot_properties: Dictionary of slot properties to set
            
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
                "widget_component_name": widget_component_name,
                "slot_properties": slot_properties
            }
            
            logger.info(f"Setting slot properties for '{widget_component_name}'")
            response = unreal.send_command("set_widget_slot_properties", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Set widget slot properties response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting widget slot properties: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    logger.info("UMG layout tools registered successfully")
