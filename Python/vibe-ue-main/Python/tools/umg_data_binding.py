"""
UMG Widget Data Binding and Dynamic Content Tools

This module provides tools for binding widgets to data sources,
creating dynamic content, and implementing MVVM patterns.
"""

import logging
from typing import Dict, Any, List, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_umg_data_binding_tools(mcp: FastMCP):
    """Register UMG data binding and dynamic content tools with the MCP server."""
    
    # Data Source Integration
    

    @mcp.tool()
    def create_data_binding_context(
        ctx: Context,
        widget_name: str,
        context_name: str,
        data_class: str
    ) -> Dict[str, Any]:
        """
        Create data binding context for MVVM pattern.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            context_name: Name for the binding context
            data_class: C++ or Blueprint class to use as data context
            
        Returns:
            Dict containing success status and context info
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "context_name": context_name,
                "data_class": data_class
            }
            
            logger.info(f"Creating data binding context '{context_name}' for widget '{widget_name}'")
            response = unreal.send_command("create_data_binding_context", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Create data binding context response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error creating data binding context: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def setup_list_item_template(
        ctx: Context,
        widget_name: str,
        list_component_name: str,
        item_template_widget: str,
        data_binding_map: Dict[str, str]
    ) -> Dict[str, Any]:
        """
        Setup data template for list/tree view items.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            list_component_name: Name of the list view component
            item_template_widget: Name of the widget to use as item template
            data_binding_map: Map of template widget properties to data properties
            
        Returns:
            Dict containing success status and template info
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "list_component_name": list_component_name,
                "item_template_widget": item_template_widget,
                "data_binding_map": data_binding_map
            }
            
            logger.info(f"Setting up list item template for '{list_component_name}'")
            response = unreal.send_command("setup_list_item_template", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Setup list item template response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting up list item template: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    # Dynamic Content Management
    @mcp.tool()
    def add_list_view(
        ctx: Context,
        widget_name: str,
        list_view_name: str,
        parent_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 200.0],
        item_height: float = 32.0,
        selection_mode: str = "Single"
    ) -> Dict[str, Any]:
        """
        Add a List View for data display.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            list_view_name: Name for the new List View
            parent_name: Name of the parent panel to add the List View to
            position: [X, Y] position of the list view
            size: [Width, Height] of the list view
            item_height: Height of each list item
            selection_mode: Selection mode ("Single", "Multi", "None")
            
        Returns:
            Dict containing success status and list view properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "list_view_name": list_view_name,
                "parent_name": parent_name,
                "position": position,
                "size": size,
                "item_height": item_height,
                "selection_mode": selection_mode
            }
            
            logger.info(f"Adding List View '{list_view_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_list_view", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add list view response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding list view: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_tile_view(
        ctx: Context,
        widget_name: str,
        tile_view_name: str,
        parent_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [400.0, 300.0],
        tile_width: float = 128.0,
        tile_height: float = 128.0
    ) -> Dict[str, Any]:
        """
        Add a Tile View for grid-based data display.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            tile_view_name: Name for the new Tile View
            parent_name: Name of the parent container to add the tile view to
            position: [X, Y] position of the tile view
            size: [Width, Height] of the tile view
            tile_width: Width of each tile
            tile_height: Height of each tile
            
        Returns:
            Dict containing success status and tile view properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "tile_view_name": tile_view_name,
                "parent_name": parent_name,
                "position": position,
                "size": size,
                "tile_width": tile_width,
                "tile_height": tile_height
            }
            
            logger.info(f"Adding Tile View '{tile_view_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_tile_view", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add tile view response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding tile view: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_tree_view(
        ctx: Context,
        widget_name: str,
        tree_view_name: str,
        parent_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 250.0],
        item_height: float = 24.0
    ) -> Dict[str, Any]:
        """
        Add a Tree View for hierarchical data.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            tree_view_name: Name for the new Tree View
            parent_name: Name of the parent container to add the tree view to
            position: [X, Y] position of the tree view
            size: [Width, Height] of the tree view
            item_height: Height of each tree item
            
        Returns:
            Dict containing success status and tree view properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "tree_view_name": tree_view_name,
                "parent_name": parent_name,
                "position": position,
                "size": size,
                "item_height": item_height
            }
            
            logger.info(f"Adding Tree View '{tree_view_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_tree_view", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add tree view response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding tree view: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def populate_list_with_data(
        ctx: Context,
        widget_name: str,
        list_component_name: str,
        data_items: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """
        Populate a list view with data items.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            list_component_name: Name of the list view component
            data_items: List of data items to populate the list with
            
        Returns:
            Dict containing success status and population info
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "list_component_name": list_component_name,
                "data_items": data_items
            }
            
            logger.info(f"Populating list '{list_component_name}' with {len(data_items)} items")
            response = unreal.send_command("populate_list_with_data", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Populate list with data response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error populating list with data: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    logger.info("UMG data binding tools registered successfully")
