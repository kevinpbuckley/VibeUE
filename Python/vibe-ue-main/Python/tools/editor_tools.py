"""
Editor Tools for Unreal MCP.

This module provides tools for controlling the Unreal Editor viewport and other editor functionality.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_editor_tools(mcp: FastMCP):
    """Register editor tools with the MCP server."""
    



    

    

    



    @mcp.tool()
    def open_asset_in_editor(
        ctx: Context,
        asset_path: str,
        force_open: bool = False
    ) -> Dict[str, Any]:
        """
        Open an asset in the Unreal Editor for editing.
        
        🎯 **ASSET EDITOR LAUNCHER**: Open any asset type in its appropriate editor.
        Perfect for quick asset inspection and editing from AI workflows.
        
        Args:
            asset_path: Full path to the asset to open
                       Examples: "/Game/Materials/M_Metal", "/Game/Textures/T_Grass"
                       Can be with or without file extension
            force_open: Whether to force open even if asset is already open
                       Default: False (will focus existing editor if already open)
            
        Returns:
            Dict containing:
            - success: boolean indicating if asset was opened
            - asset_path: path to the asset that was opened
            - editor_type: type of editor that was opened
            - was_already_open: whether asset was already open
            - error: string (only if success=false)
            
        📝 **Supported Asset Types**:
        - Textures (T_*) → Texture Editor
        - Materials (M_*, MI_*) → Material/Material Instance Editor  
        - Blueprints (BP_*, WBP_*) → Blueprint Editor
        - Static Meshes (SM_*) → Static Mesh Editor
        - Audio (A_*, S_*) → Audio Editor
        - Data Tables (DT_*) → Data Table Editor
        - And many more...
        
        💡 **Usage Examples**:
        ```python
        # Open a texture for editing
        open_asset_in_editor("/Game/Textures/UI/T_ButtonNormal")
        
        # Open a material
        open_asset_in_editor("/Game/Materials/M_MetalShiny")
        
        # Open a widget blueprint
        open_asset_in_editor("/Game/UI/WBP_MainMenu")
        
        # Force open even if already open
        open_asset_in_editor("/Game/Blueprints/BP_Player", force_open=True)
        ```
        
        🔧 **AI Integration Tips**:
        - Use after search_items() to inspect found assets
        - Perfect for texture analysis before applying to widgets
        - Useful for material parameter verification
        - Great for Blueprint inspection and modification
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {
                    "success": False, 
                    "error": "Failed to connect to Unreal Engine",
                    "asset_path": asset_path
                }
                
            # Prepare parameters
            params = {
                "asset_path": asset_path,
                "force_open": force_open
            }
                
            logger.info(f"Opening asset in editor: {asset_path}")
            # Try the canonical command first, then fall back to legacy/lowercase names
            response = unreal.send_command("OpenAssetInEditor", params)

            # If Unreal doesn't recognize the command, try a lowercase fallback used by older plugins
            if response and (response.get("status") == "error" and "Unknown command" in (response.get("error") or "")):
                logger.warning("OpenAssetInEditor not recognized by Unreal, retrying with 'open_asset_in_editor' fallback")
                response = unreal.send_command("open_asset_in_editor", params)

            # Normalise response shapes. Unreal may return either a top-level success or nested result object.
            # If we got the wrapper {"status":"success","result":{...}}, unwrap it.
            if response and isinstance(response, dict) and response.get("status") == "success" and isinstance(response.get("result"), dict):
                response = response.get("result")

            # Also accept older style {"success": true, ...} at top level
            if response and response.get("success"):
                logger.info(f"Successfully opened asset: {asset_path}")
                return {
                    "success": True,
                    "asset_path": asset_path,
                    "editor_type": response.get("editor_type", response.get("message", "Unknown")),
                    "was_already_open": response.get("was_already_open", False)
                }

            # Otherwise build a helpful error message
            error_msg = None
            if response:
                # Prefer explicit error fields
                error_msg = response.get("error") or response.get("message")
                # Some older handlers return an object with status="error"
                if not error_msg and response.get("status") == "error":
                    error_msg = response.get("error") or response.get("message")
            if not error_msg:
                error_msg = "Unknown error opening asset"

            logger.error(f"Failed to open asset {asset_path}: {error_msg}")
            return {
                "success": False,
                "error": error_msg,
                "asset_path": asset_path
            }
            
        except Exception as e:
            logger.error(f"Error opening asset in editor: {e}")
            return {
                "success": False, 
                "error": str(e),
                "asset_path": asset_path
            }

    logger.info("Editor tools registered successfully")
