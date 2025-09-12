"""
UMG Widget Components Tools

⚠️ **CRITICAL FOR AI**: Before adding any components for styling or backgrounds, 
ALWAYS call get_umg_guide() first to understand proper widget hierarchy, 
container-specific requirements, and component placement patterns.

This module provides tools for adding various types of UMG widget components including
basic controls, input widgets, media widgets, and layout containers.
"""

import logging
from typing import Dict, Any, List, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_umg_component_tools(mcp: FastMCP):
    """
    Register UMG component creation tools with the MCP server.
    
    ⚠️ **AI INTEGRATION NOTE**: Before adding components (especially for backgrounds 
    and styling), AI assistants should call get_umg_guide() first to understand 
    proper implementation patterns and container requirements.
    """
    
    # Text & Input Widgets
    @mcp.tool()
    def add_editable_text(
        ctx: Context,
        widget_name: str,
        editable_text_name: str,
        parent_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 30.0],
        font_size: int = 12,
        color: List[float] = [1.0, 1.0, 1.0, 1.0],
        hint_text: str = ""
    ) -> Dict[str, Any]:
        """
        Add an Editable Text widget for single-line text input.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            editable_text_name: Name for the new Editable Text widget
            parent_name: Name of the parent panel to add the Editable Text to
            text: Initial text content
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the text input
            font_size: Font size in points
            color: [R, G, B, A] text color values (0.0 to 1.0)
            hint_text: Placeholder text when empty
            
        Returns:
            Dict containing success status and widget properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "editable_text_name": editable_text_name,
                "parent_name": parent_name,
                "text": text,
                "position": position,
                "size": size,
                "font_size": font_size,
                "color": color,
                "hint_text": hint_text
            }
            
            logger.info(f"Adding Editable Text '{editable_text_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_editable_text", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add editable text response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding editable text: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_editable_text_box(
        ctx: Context,
        widget_name: str,
        text_box_name: str,
        parent_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 100.0],
        font_size: int = 12,
        color: List[float] = [1.0, 1.0, 1.0, 1.0],
        is_multiline: bool = True,
        is_read_only: bool = False
    ) -> Dict[str, Any]:
        """
        Add an Editable Text Box widget for multi-line text input.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            text_box_name: Name for the new Editable Text Box widget
            parent_name: Name of the parent panel to add the Editable Text Box to
            text: Initial text content
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the text box
            font_size: Font size in points
            color: [R, G, B, A] text color values (0.0 to 1.0)
            is_multiline: Whether to allow multiple lines
            is_read_only: Whether the text box is read-only
            
        Returns:
            Dict containing success status and widget properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "text_box_name": text_box_name,
                "parent_name": parent_name,
                "text": text,
                "position": position,
                "size": size,
                "font_size": font_size,
                "color": color,
                "is_multiline": is_multiline,
                "is_read_only": is_read_only
            }
            
            logger.info(f"Adding Editable Text Box '{text_box_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_editable_text_box", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add editable text box response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding editable text box: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_rich_text_block(
        ctx: Context,
        widget_name: str,
        rich_text_name: str,
        parent_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [400.0, 100.0],
        font_size: int = 12,
        color: List[float] = [1.0, 1.0, 1.0, 1.0],
        auto_wrap_text: bool = True
    ) -> Dict[str, Any]:
        """
        Add a Rich Text Block widget for formatted text with markup support.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            rich_text_name: Name for the new Rich Text Block widget
            parent_name: Name of the parent panel to add the Rich Text Block to
            text: Initial text content (supports markup)
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the text block
            font_size: Font size in points
            color: [R, G, B, A] text color values (0.0 to 1.0)
            auto_wrap_text: Whether to wrap text automatically
            
        Returns:
            Dict containing success status and widget properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "rich_text_name": rich_text_name,
                "parent_name": parent_name,
                "text": text,
                "position": position,
                "size": size,
                "font_size": font_size,
                "color": color,
                "auto_wrap_text": auto_wrap_text
            }
            
            logger.info(f"Adding Rich Text Block '{rich_text_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_rich_text_block", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add rich text block response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding rich text block: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    # Interactive Widgets
    @mcp.tool()
    def add_check_box(
        ctx: Context,
        widget_name: str,
        check_box_name: str,
        parent_name: str,
        is_checked: bool = False,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [20.0, 20.0],
        label_text: str = "",
        checked_state: str = "Checked"
    ) -> Dict[str, Any]:
        """
        Add a Check Box widget with optional label.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            check_box_name: Name for the new Check Box widget
            parent_name: Name of the parent panel to add the Check Box to
            is_checked: Initial checked state
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the check box
            label_text: Optional label text next to checkbox
            checked_state: State type ("Checked", "Unchecked", "Undetermined")
            
        Returns:
            Dict containing success status and widget properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "check_box_name": check_box_name,
                "parent_name": parent_name,
                "is_checked": is_checked,
                "position": position,
                "size": size,
                "label_text": label_text,
                "checked_state": checked_state
            }
            
            logger.info(f"Adding Check Box '{check_box_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_check_box", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add check box response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding check box: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_slider(
        ctx: Context,
        widget_name: str,
        slider_name: str,
        parent_name: str,
        value: float = 0.5,
        min_value: float = 0.0,
        max_value: float = 1.0,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 20.0],
        orientation: str = "Horizontal",
        step_size: float = 0.01
    ) -> Dict[str, Any]:
        """
        Add a Slider widget for value selection.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            slider_name: Name for the new Slider widget
            parent_name: Name of the parent panel to add the Slider to
            value: Initial value
            min_value: Minimum slider value
            max_value: Maximum slider value
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the slider
            orientation: Slider orientation ("Horizontal" or "Vertical")
            step_size: Increment step size
            
        Returns:
            Dict containing success status and widget properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "slider_name": slider_name,
                "parent_name": parent_name,
                "value": value,
                "min_value": min_value,
                "max_value": max_value,
                "position": position,
                "size": size,
                "orientation": orientation,
                "step_size": step_size
            }
            
            logger.info(f"Adding Slider '{slider_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_slider", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add slider response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding slider: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_progress_bar(
        ctx: Context,
        widget_name: str,
        progress_bar_name: str,
        parent_name: str,
        percent: float = 0.0,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 20.0],
        fill_color: List[float] = [0.0, 1.0, 0.0, 1.0],
        background_color: List[float] = [0.2, 0.2, 0.2, 1.0]
    ) -> Dict[str, Any]:
        """
        Add a Progress Bar widget for indicating progress.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            progress_bar_name: Name for the new Progress Bar widget
            parent_name: Name of the parent panel to add the Progress Bar to
            percent: Initial progress percentage (0.0 to 1.0)
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the progress bar
            fill_color: [R, G, B, A] fill color values (0.0 to 1.0)
            background_color: [R, G, B, A] background color values (0.0 to 1.0)
            
        Returns:
            Dict containing success status and widget properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "progress_bar_name": progress_bar_name,
                "parent_name": parent_name,
                "percent": percent,
                "position": position,
                "size": size,
                "fill_color": fill_color,
                "background_color": background_color
            }
            
            logger.info(f"Adding Progress Bar '{progress_bar_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_progress_bar", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add progress bar response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding progress bar: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    # Media & Visual Widgets
    @mcp.tool()
    def add_image(
        ctx: Context,
        widget_name: str,
        image_name: str,
        parent_name: str,
        image_path: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [100.0, 100.0],
        color_tint: List[float] = [1.0, 1.0, 1.0, 1.0]
    ) -> Dict[str, Any]:
        """
        Add an Image widget for displaying static images.
        
        ⚠️ **IMPORTANT**: Before adding images for backgrounds, ALWAYS use get_umg_guide() 
        first to understand proper container-specific background implementation, sizing 
        requirements, and slot property configuration.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            image_name: Name for the new Image widget
            parent_name: Name of the parent panel to add the Image to
            image_path: Path to the image asset (optional)
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the image
            color_tint: [R, G, B, A] color tint values (0.0 to 1.0)
            
        Returns:
            Dict containing success status and widget properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "image_name": image_name,
                "parent_name": parent_name,
                "image_path": image_path,
                "position": position,
                "size": size,
                "color_tint": color_tint
            }
            
            logger.info(f"Adding Image '{image_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_image", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add image response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding image: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_spacer(
        ctx: Context,
        widget_name: str,
        spacer_name: str,
        parent_name: str,
        size: List[float] = [50.0, 50.0],
        position: List[float] = [0.0, 0.0]
    ) -> Dict[str, Any]:
        """
        Add a Spacer widget for layout spacing.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            spacer_name: Name for the new Spacer widget
            parent_name: Name of the parent panel to add the Spacer to
            size: [Width, Height] of the spacer
            position: [X, Y] position in the canvas panel
            
        Returns:
            Dict containing success status and widget properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "spacer_name": spacer_name,
                "parent_name": parent_name,
                "size": size,
                "position": position
            }
            
            logger.info(f"Adding Spacer '{spacer_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_spacer", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add spacer response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding spacer: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    # Widget Switcher and Container Widgets
    @mcp.tool()
    def add_widget_switcher(
        ctx: Context,
        widget_name: str,
        switcher_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [400.0, 300.0],
        active_widget_index: int = 0
    ) -> Dict[str, Any]:
        """
        Add Widget Switcher for tab-like behavior.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            switcher_name: Name for the new Widget Switcher
            position: [X, Y] position of the switcher
            size: [Width, Height] of the switcher
            active_widget_index: Index of initially active widget
            
        Returns:
            Dict containing success status and switcher properties
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "switcher_name": switcher_name,
                "position": position,
                "size": size,
                "active_widget_index": active_widget_index
            }
            
            logger.info(f"Adding Widget Switcher '{switcher_name}' to widget '{widget_name}'")
            response = unreal.send_command("add_widget_switcher", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add widget switcher response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding widget switcher: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    @mcp.tool()
    def add_widget_switcher_slot(
        ctx: Context,
        widget_name: str,
        switcher_name: str,
        child_widget_name: str,
        slot_index: int
    ) -> Dict[str, Any]:
        """
        Add slot to Widget Switcher.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            switcher_name: Name of the Widget Switcher
            child_widget_name: Name of the child widget to add
            slot_index: Index position for the slot
            
        Returns:
            Dict containing success status and slot info
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "switcher_name": switcher_name,
                "child_widget_name": child_widget_name,
                "slot_index": slot_index
            }
            
            logger.info(f"Adding slot to Widget Switcher '{switcher_name}'")
            response = unreal.send_command("add_widget_switcher_slot", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Add widget switcher slot response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding widget switcher slot: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

    logger.info("UMG component tools registered successfully")
