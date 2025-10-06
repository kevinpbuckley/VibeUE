"""
UMG Tools for Unreal MCP.

This module provides tools for creating and manipulating UMG Widget Blueprints in Unreal Engine.
"""

import logging
from typing import Dict, List, Any
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_umg_tools(mcp: FastMCP):
    """Register UMG tools with the MCP server."""

    @mcp.tool()
    def create_umg_widget_blueprint(
        ctx: Context,
        name: str,
        parent_class: str = "UserWidget",
        path: str = "/Game/UI"
    ) -> Dict[str, Any]:
        """
        Create a new UMG Widget Blueprint.
        
        ⚠️ CRITICAL DEPENDENCY ORDER: After creating the Widget Blueprint, you MUST create elements in this order:
        1) Variables FIRST - Create all Blueprint variables before any Event Graph nodes
        2) Widget Components SECOND - Add all UMG components (Button, TextBlock, etc.)  
        3) Functions THIRD - Implement all custom functions
        4) Event Graph nodes LAST - Create logic that references the above elements
        
        This order prevents dependency failures and ensures proper Blueprint compilation.
        
        Args:
            name: Name of the widget blueprint to create
            parent_class: Parent class for the widget (default: UserWidget)
            path: Content browser path where the widget should be created
            
        Returns:
            Dict containing success status and widget path
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "name": name,
                "parent_class": parent_class,
                "path": path
            }
            
            logger.info(f"Creating UMG Widget Blueprint with params: {params}")
            response = unreal.send_command("create_umg_widget_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Create UMG Widget Blueprint response: {response}")
            
            # Add dependency order reminder to response
            result = response or {}
            if result.get("path") or result.get("name"):  # If Widget Blueprint was created successfully
                logger.info("⚠️ REMINDER: Create Widget Blueprint elements in DEPENDENCY ORDER: 1) Variables FIRST, 2) Widget Components SECOND, 3) Functions THIRD, 4) Event Graph nodes LAST")
                result["reminder"] = "Create in order: Variables → Widget Components → Functions → Event Graph"
                result["critical_order"] = "Variables FIRST, then Widget Components, then Functions, then Event Graph LAST"
            
            return result
            
        except Exception as e:
            error_msg = f"Error creating UMG Widget Blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    # ❌ REMOVED: add_text_block_to_widget() - Use add_widget_component(component_type="TextBlock") instead
    # ❌ REMOVED: add_button_to_widget() - Use add_widget_component(component_type="Button") instead
    # These tools are now obsolete due to the reflection-based UMG system.

    @mcp.tool()
    def remove_umg_component(
        ctx: Context,
        widget_name: str,
        component_name: str,
        remove_children: bool = True,
        remove_from_variables: bool = True
    ) -> Dict[str, Any]:
        """
        Universal widget component removal from UMG Widget Blueprints.
        
        🎯 **UNIVERSAL REMOVAL**: Remove any widget component from anywhere in the hierarchy:
        - Root level components
        - Children of any panel type (Canvas, Overlay, Vertical/Horizontal Box, etc.)
        - Nested components at any depth
        - Components with or without children
        
        Args:
            widget_name: Name of the target Widget Blueprint
                        ⚠️ **Must be exact name from search_items() results**
            component_name: Name of the component to remove
                           ⚠️ **Must be exact name from list_widget_components() results**
            remove_children: Also remove all child components recursively
                            Examples: True removes entire subtree, False orphans children
            remove_from_variables: Remove component variable reference from Blueprint
                                  Examples: True cleans up completely, False keeps variable stub
            
        Returns:
            Dict containing:
            - success: boolean indicating if removal completed
            - removed_components: array of all components that were removed
            - orphaned_children: array of children that were reparented to root (if remove_children=False)
            - variable_cleanup: whether component variable was removed
            - parent_info: information about the parent component that contained the removed component
            - error: string (only if success=false)
            
        📋 **Removal Capabilities**:
        - **From Root**: Remove top-level components (SizeBox, CanvasPanel, etc.)
        - **From Panels**: Remove from any panel type automatically detected
        - **From Slots**: Properly handle slot-based layouts (Grid, Uniform Grid)
        - **Recursive**: Remove entire component subtrees
        - **Variable Cleanup**: Remove Blueprint variable references
        - **Hierarchy Preservation**: Option to reparent children instead of deleting
        
        🔄 **Smart Detection**:
        - Automatically detects parent type (CanvasPanel, Overlay, VerticalBox, etc.)
        - Handles different slot types (OverlaySlot, CanvasPanelSlot, BoxSlot, etc.)
        - Preserves hierarchy integrity after removal
        - Updates widget tree structure properly
        
        💡 **Usage Examples**:
        ```python
        # Remove a button and all its children
        remove_umg_component("WBP_Inventory", "CloseButton", remove_children=True)
        
        # Remove background image but keep variable reference
        remove_umg_component("WBP_Menu", "BackgroundImage", remove_from_variables=False)
        
        # Remove panel but reparent children to root
        remove_umg_component("WBP_HUD", "StatusPanel", remove_children=False)
        
        # Remove deeply nested component
        remove_umg_component("WBP_Complex", "InnerContainer_SubPanel_Text", remove_children=True)
        ```
        
        ⚠️ **AI Best Practices**:
        1. Use list_widget_components() first to get exact component names
        2. Consider impact on layout when removing panel components
        3. Use remove_children=False to preserve child components
        4. Check component hierarchy before removal to understand impact
        5. Test removal on less critical components first
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "component_name": component_name,
                "remove_children": remove_children,
                "remove_from_variables": remove_from_variables
            }
            
            logger.info(f"Removing UMG component '{component_name}' from widget '{widget_name}'")
            response = unreal.send_command("remove_umg_component", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Remove UMG component response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error removing UMG component: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def delete_widget_blueprint(
        ctx: Context,
        widget_name: str,
        check_references: bool = True
    ) -> Dict[str, Any]:
        """
        Delete a Widget Blueprint asset entirely.
        
        🗑️ **COMPLETE ASSET DELETION**: Remove entire Widget Blueprint from the project
        with reference checking and safety validation.
        
        Args:
            widget_name: Name of the Widget Blueprint to delete
                        ⚠️ **Must be exact name from search_items() results**
            check_references: Check for references in other Blueprints before deletion
                            Examples: True (safe, default) = scan for references first,
                                     False = delete immediately without reference check
            
        Returns:
            Dict containing:
            - success: boolean indicating if deletion completed
            - widget_name: name of widget that was deleted
            - asset_path: full path to the deleted asset
            - references_found: array of other assets that referenced this widget
            - reference_count: number of references found (if check_references=True)
            - deletion_blocked: whether deletion was prevented due to references
            - error: string (only if success=false)
            
        🔒 **Safety Features**:
        - **Reference Detection**: Finds all Blueprint references before deletion
        - **Asset Path Validation**: Confirms asset exists before attempting deletion
        - **Editor Integration**: Handles assets open in editors properly
        - **Undo Support**: Deletion can be undone through editor undo system
        
        💡 **Usage Examples**:
        ```python
        # Safe deletion with reference checking
        delete_widget_blueprint("WBP_TestWidget", check_references=True)
        
        # Force deletion without reference check (use carefully)
        delete_widget_blueprint("WBP_TempWidget", check_references=False)
        
        # Delete widget found through search
        widgets = search_items("TestWidget", "Widget")
        delete_widget_blueprint(widgets["items"][0]["name"], check_references=True)
        ```
        
        ⚠️ **AI Best Practices**:
        1. Use search_items() first to get exact widget name
        2. Always use check_references=True for safety (default)
        3. Review reference list before forcing deletion
        4. Cannot delete widgets that are currently open in editor
        5. Test deletion on non-critical widgets first
        
        🚫 **Limitations**:
        - Cannot delete while widget is open in Blueprint editor
        - Cannot delete engine or plugin widgets
        - References in C++ code won't be detected
        - Deletion is permanent (use version control for safety)
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "check_references": check_references
            }
            
            logger.info(f"Deleting Widget Blueprint '{widget_name}' (check_references={check_references})")
            response = unreal.send_command("delete_widget_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Widget Blueprint deletion response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error deleting Widget Blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

