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
            return response
            
        except Exception as e:
            error_msg = f"Error creating UMG Widget Blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    # ❌ REMOVED: add_text_block_to_widget() - Use add_widget_component(component_type="TextBlock") instead
    # ❌ REMOVED: add_button_to_widget() - Use add_widget_component(component_type="Button") instead
    # These tools are now obsolete due to the reflection-based UMG system.


    @mcp.tool()
    def get_umg_guide(ctx: Context) -> Dict[str, Any]:
        """
        Get comprehensive UMG styling guide by reading from UMG-Guide.md documentation.
        
        AI Usage: Use this for foundational understanding of UMG component relationships and detailed
        guidance on implementing backgrounds and styling in different container types.
        
        Returns:
            Dict containing the complete styling guide content from UMG-Guide.md
        """
        import os
        
        try:
            # Get the path to the UMG-Guide.md file
            current_dir = os.path.dirname(os.path.abspath(__file__))
            # Instead of assuming a fixed number of parent levels, search upwards
            # for a `resources/UMG-Guide.md` file to be robust to different layouts.
            guide_path = None
            search_dir = current_dir
            for _ in range(8):  # search up to 8 levels
                candidate = os.path.join(search_dir, '..', 'resources', 'UMG-Guide.md')
                candidate = os.path.abspath(candidate)
                if os.path.isfile(candidate):
                    guide_path = candidate
                    break
                # move one level up
                parent = os.path.abspath(os.path.join(search_dir, '..'))
                if parent == search_dir:
                    break
                search_dir = parent

            if guide_path is None:
                # Fallback: attempt the expected project-relative location
                project_root = os.path.abspath(os.path.join(current_dir, '..', '..', '..', '..'))
                guide_path = os.path.abspath(os.path.join(project_root, 'Plugins', 'VibeUE', 'Python', 'vibe-ue-main', 'Python', 'resources', 'AIStyleGuide.md'))
            
            # Read the markdown file
            with open(guide_path, 'r', encoding='utf-8') as f:
                guide_content = f.read()
            
            return {
                "success": True,
                "guide_content": guide_content,
                "source_file": "UMG-Guide.md",
                "file_path": guide_path,
                "content_type": "markdown",
                "description": "Complete UMG styling guide covering Widgets, Panels, Slots, and background implementation",
                "sections": [
                    "Part 1: UMG Foundation - Understanding the Component System",
                    "Part 2: Container Background Implementation", 
                    "Part 3: Practical Styling Patterns",
                    "Implementation Examples",
                    "Comprehensive Implementation Checklist"
                ]
            }
            
        except FileNotFoundError:
            return {
                "success": False,
                "error": f"UMG-Guide.md not found at expected location: {guide_path}",
                "troubleshooting": "Ensure the UMG-Guide.md file exists in the resources directory"
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Error reading UMG-Guide.md: {str(e)}",
                "troubleshooting": "Check file permissions and encoding"
            }

