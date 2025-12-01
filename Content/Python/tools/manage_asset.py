"""
Asset Manager - Unified multi-action tool for asset operations.

Consolidates asset import, export, conversion, and editor operations.
"""

import logging
from typing import Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

# Valid actions for the manage_asset tool
VALID_ACTIONS = [
    "search",
    "import_texture",
    "export_texture",
    "delete",
    "open_in_editor",
    "svg_to_png",
    "duplicate",
    "save",
    "save_all",
    "list_references"
]


def register_asset_tools(mcp: FastMCP):
    """Register unified asset manager tool with the MCP server."""

    @mcp.tool()
    def manage_asset(
        ctx: Context,
        action: str,
        # Search parameters
        search_term: str = "",
        asset_type: str = "",
        path: str = "/Game",
        case_sensitive: bool = False,
        include_engine_content: bool = False,
        max_results: int = 100,
        # Import parameters
        file_path: str = "",
        destination_path: str = "/Game/Textures/Imported",
        texture_name: Optional[str] = None,
        compression_settings: str = "TC_Default",
        generate_mipmaps: bool = True,
        validate_format: bool = True,
        auto_optimize: bool = True,
        convert_if_needed: bool = False,
        raster_size: Optional[List[int]] = None,
        auto_convert_svg: bool = True,
        # Export parameters
        asset_path: str = "",
        export_format: str = "PNG",
        max_size: Optional[List[int]] = None,
        temp_folder: str = "",
        # Open in editor parameters
        force_open: bool = False,
        # Delete parameters
        force_delete: bool = False,
        show_confirmation: bool = True,
        # SVG conversion parameters
        svg_path: str = "",
        output_path: Optional[str] = None,
        size: Optional[List[int]] = None,
        scale: float = 1.0,
        background: Optional[str] = None,
        # Duplicate parameters
        new_name: str = "",
        # Save parameters
        prompt_user: bool = False,
        # Reference parameters
        include_dependencies: bool = False
    ) -> Dict[str, Any]:
        """
        Single multi-action tool for all asset operations.
        
         **Available Actions:**
        **search** - Search for assets in the project (widgets, textures, blueprints, materials, etc.)
        ```python
        manage_asset(
            action="search",
            search_term="radar",
            asset_type="Widget"
        )
        # Returns list of matching assets with paths and metadata
        ```
        
        **import_texture** - Import texture from file system
        ```python
        manage_asset(
            action="import_texture",
            file_path="C:/Path/To/texture.png",
            destination_path="/Game/Textures/UI",
            texture_name="T_ButtonNormal",
            compression_settings="TC_Default"
        )
        ```
        
        **export_texture** - Export texture for AI analysis
        ```python
        manage_asset(
            action="export_texture",
            asset_path="/Game/Textures/T_Background",
            export_format="PNG",
            max_size=[256, 256]
        )
        # Returns temp file path for AI to analyze visually
        ```
        
        **open_in_editor** - Open asset in appropriate editor
        ```python
        manage_asset(
            action="open_in_editor",
            asset_path="/Game/Materials/M_Metal",
            force_open=False
        )
        ```
        
        **delete** - Delete asset from project with safety checks
        ```python
        # Safe deletion with user confirmation (default)
        manage_asset(
            action="delete",
            asset_path="/Game/Textures/T_OldTexture",
            force_delete=False,       # Will error if asset has references
            show_confirmation=True    # Shows confirmation dialog
        )
        
        # Automated deletion without user interaction
        manage_asset(
            action="delete",
            asset_path="/Game/Textures/T_OldTexture",
            force_delete=True,        # Delete even if asset has references
            show_confirmation=False   # No confirmation dialog (instant deletion)
        )
        
        # ⚠️ CRITICAL: force_delete and show_confirmation are DIRECT parameters
        # DO NOT pass them in extra dict - they will be ignored!
        
        # Returns confirmation of deletion or error if asset in use
        # Error codes:
        # - ASSET_NOT_FOUND: Asset doesn't exist
        # - ASSET_IN_USE: Asset has references (use force_delete=True to override)
        # - ASSET_READ_ONLY: Asset is engine content (cannot delete)
        # - OPERATION_CANCELLED: User cancelled confirmation dialog
        # - ASSET_DELETE_FAILED: Deletion operation failed
        ```
        
        **svg_to_png** - Convert SVG to PNG
        ```python
        manage_asset(
            action="svg_to_png",
            svg_path="C:/path/icon.svg",
            output_path="C:/path/icon.png",
            size=[512, 512],
            background="#00000000"
        )
        ```
        
        **duplicate** - Duplicate an existing asset to a new location
        ```python
        manage_asset(
            action="duplicate",
            asset_path="/Game/Blueprints/BP_Player",
            destination_path="/Game/Blueprints/Characters",
            new_name="BP_Player2"
        )
        # Returns new asset path and type
        ```
        
        **save** - Save a single asset to disk
        ```python
        manage_asset(
            action="save",
            asset_path="/Game/Blueprints/BP_Player"
        )
        # Saves the specified asset and marks package as saved
        ```
        
        **save_all** - Save all dirty (modified) assets
        ```python
        # Save all without prompting user
        manage_asset(
            action="save_all",
            prompt_user=False
        )
        
        # Save all with user confirmation dialog
        manage_asset(
            action="save_all",
            prompt_user=True
        )
        # Returns count of assets saved
        ```
        
        **list_references** - Get all assets that reference this asset (and optionally dependencies)
        ```python
        # Get assets referencing this asset
        manage_asset(
            action="list_references",
            asset_path="/Game/Input/Actions/IA_Move"
        )
        # Returns:
        # {
        #     "success": true,
        #     "asset_path": "/Game/Input/Actions/IA_Move",
        #     "referencers": ["/Game/Input/IMC_Default", "/Game/Blueprints/BP_Player"],
        #     "referencer_count": 2,
        #     "dependencies": [],
        #     "dependency_count": 0
        # }
        
        # Include what this asset depends on
        manage_asset(
            action="list_references",
            asset_path="/Game/Input/IMC_Default",
            include_dependencies=True
        )
        # Returns referencers AND dependencies lists
        ```
        
        Args:
            action: Action to perform (search|import_texture|export_texture|delete|open_in_editor|svg_to_png|duplicate|save|save_all|list_references)
            search_term: Text to search for in asset names (for search)
            asset_type: Filter by asset type (for search - examples: Widget, Texture2D, Material, Blueprint)
            path: Content browser path to search in (for search, default: /Game)
            case_sensitive: Whether search should be case sensitive (for search)
            include_engine_content: Whether to include engine content (for search)
            max_results: Maximum number of results to return (for search)
            file_path: Source file path (for import_texture)
            asset_path: Asset path in project (for export_texture, delete, open_in_editor, duplicate)
            svg_path: SVG file path (for svg_to_png)
            destination_path: Destination in content browser (for import_texture, duplicate)
            texture_name: Custom texture name (for import_texture)
            compression_settings: Texture compression (for import_texture)
            export_format: Export format (for export_texture)
            max_size: Maximum export size (for export_texture, svg_to_png)
            force_open: Force open if already open (for open_in_editor)
            force_delete: Delete even if asset has references (for delete action, default: False)
                         When False: returns error if asset is referenced by other assets
                         When True: deletes asset and breaks references
            show_confirmation: Show confirmation dialog before deletion (for delete action, default: True)
                              When True: displays dialog requiring user click (may timeout MCP call)
                              When False: instant deletion without user interaction (recommended for automation)
                              ⚠️ Must be passed as direct parameter, NOT in extra dict
            output_path: Output file path (for svg_to_png)
            size: Output size (for svg_to_png)
            scale: Scale multiplier (for svg_to_png)
            background: Background color (for svg_to_png)
            new_name: New asset name (for duplicate - optional, defaults to source name with suffix)
            prompt_user: Show save confirmation dialog (for save_all action, default: False)
                        When True: displays dialog requiring user confirmation
                        When False: saves all automatically without user interaction
            include_dependencies: Include outgoing dependencies (for list_references, default: False)
                        When False: only returns assets that reference this asset
                        When True: also returns assets this asset depends on
            
        Returns:
            Dict containing action results with success field
        """
        action = action.lower()
        
        # Route to appropriate handler
        if action == "search":
            return _handle_search(
                search_term, asset_type, path, case_sensitive,
                include_engine_content, max_results
            )
        elif action == "import_texture":
            return _handle_import_texture(
                file_path, destination_path, texture_name, compression_settings,
                generate_mipmaps, validate_format, auto_optimize, convert_if_needed,
                raster_size, auto_convert_svg
            )
        elif action == "export_texture":
            return _handle_export_texture(
                asset_path, export_format, max_size, temp_folder
            )
        elif action == "delete":
            return _handle_delete_asset(
                asset_path, force_delete, show_confirmation
            )
        elif action == "open_in_editor":
            return _handle_open_in_editor(asset_path, force_open)
        elif action == "svg_to_png":
            return _handle_convert_svg(
                svg_path, output_path, size, scale, background
            )
        elif action == "duplicate":
            return _handle_duplicate_asset(
                asset_path, destination_path, new_name
            )
        elif action == "save":
            return _handle_save_asset(asset_path)
        elif action == "save_all":
            return _handle_save_all_assets(prompt_user)
        elif action == "list_references":
            return _handle_list_references(asset_path, include_dependencies)
        else:
            return {
                "success": False,
                "error": f"Unknown action '{action}'. Valid actions: {', '.join(VALID_ACTIONS)}"
            }


def _handle_search(
    search_term: str,
    asset_type: str,
    path: str,
    case_sensitive: bool,
    include_engine_content: bool,
    max_results: int
) -> Dict[str, Any]:
    """Handle asset search."""
    from vibe_ue_server import get_unreal_connection
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Searching for assets: term='{search_term}', type='{asset_type}', path='{path}'")
        response = unreal.send_command("search_items", {
            "search_term": search_term,
            "asset_type": asset_type,
            "path": path,
            "case_sensitive": case_sensitive,
            "include_engine_content": include_engine_content,
            "max_results": max_results
        })
        
        if not response:
            return {"success": False, "error": "No response from Unreal Engine"}
        
        return response
        
    except Exception as e:
        logger.error(f"Error searching assets: {e}")
        return {"success": False, "error": str(e)}


def _handle_import_texture(
    file_path: str,
    destination_path: str,
    texture_name: Optional[str],
    compression_settings: str,
    generate_mipmaps: bool,
    validate_format: bool,
    auto_optimize: bool,
    convert_if_needed: bool,
    raster_size: Optional[List[int]],
    auto_convert_svg: bool
) -> Dict[str, Any]:
    """Handle texture import."""
    from vibe_ue_server import get_unreal_connection
    
    if not file_path:
        return {"success": False, "error": "'file_path' is required for import_texture action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Importing texture from {file_path}")
        response = unreal.send_command("import_texture_asset", {
            "file_path": file_path,
            "destination_path": destination_path,
            "texture_name": texture_name,
            "compression_settings": compression_settings,
            "generate_mipmaps": generate_mipmaps,
            "validate_format": validate_format,
            "auto_optimize": auto_optimize,
            "convert_if_needed": convert_if_needed,
            "raster_size": raster_size,
            "auto_convert_svg": auto_convert_svg
        })
        
        if not response:
            return {"success": False, "error": "No response from Unreal Engine"}
        
        return response
        
    except Exception as e:
        logger.error(f"Error importing texture: {e}")
        return {"success": False, "error": str(e)}


def _handle_export_texture(
    asset_path: str,
    export_format: str,
    max_size: Optional[List[int]],
    temp_folder: str
) -> Dict[str, Any]:
    """Handle texture export for analysis."""
    from vibe_ue_server import get_unreal_connection
    
    if not asset_path:
        return {"success": False, "error": "'asset_path' is required for export_texture action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Exporting texture {asset_path} for analysis")
        response = unreal.send_command("export_texture_for_analysis", {
            "asset_path": asset_path,
            "export_format": export_format,
            "max_size": max_size,
            "temp_folder": temp_folder
        })
        
        if not response:
            return {"success": False, "error": "No response from Unreal Engine"}
        
        return response
        
    except Exception as e:
        logger.error(f"Error exporting texture: {e}")
        return {"success": False, "error": str(e)}


def _handle_delete_asset(
    asset_path: str,
    force_delete: bool,
    show_confirmation: bool
) -> Dict[str, Any]:
    """Handle asset deletion with safety checks."""
    from vibe_ue_server import get_unreal_connection
    
    if not asset_path:
        return {"success": False, "error": "'asset_path' is required for delete action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Deleting asset: {asset_path} (force_delete={force_delete}, show_confirmation={show_confirmation})")
        response = unreal.send_command("delete_asset", {
            "asset_path": asset_path,
            "force_delete": force_delete,
            "show_confirmation": show_confirmation
        })
        
        if not response:
            return {"success": False, "error": "No response from Unreal Engine"}
        
        return response
        
    except Exception as e:
        logger.error(f"Error deleting asset: {e}")
        return {"success": False, "error": str(e)}


def _handle_open_in_editor(asset_path: str, force_open: bool) -> Dict[str, Any]:
    """Handle opening asset in editor."""
    from vibe_ue_server import get_unreal_connection
    
    if not asset_path:
        return {"success": False, "error": "'asset_path' is required for open_in_editor action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Opening asset {asset_path} in editor")
        response = unreal.send_command("OpenAssetInEditor", {
            "asset_path": asset_path,
            "force_open": force_open
        })
        
        if not response:
            return {"success": False, "error": "No response from Unreal Engine"}
        
        return response
        
    except Exception as e:
        logger.error(f"Error opening asset in editor: {e}")
        return {"success": False, "error": str(e)}


def _handle_convert_svg(
    svg_path: str,
    output_path: Optional[str],
    size: Optional[List[int]],
    scale: float,
    background: Optional[str]
) -> Dict[str, Any]:
    """Handle SVG to PNG conversion."""
    import io
    from pathlib import Path
    
    if not svg_path:
        return {"success": False, "error": "'svg_path' is required for svg_to_png action"}
    
    try:
        # Import dependencies
        try:
            import cairosvg
            from PIL import Image
            import numpy as np
        except ImportError as e:
            return {
                "success": False,
                "error": f"Missing required package: {e}. Install with: pip install cairosvg pillow numpy",
                "png_path": None
            }
        
        # Validate input
        svg_file = Path(svg_path)
        if not svg_file.exists():
            return {
                "success": False,
                "error": f"SVG file not found: {svg_path}",
                "png_path": None
            }
        
        # Determine output path
        if output_path:
            png_file = Path(output_path)
        else:
            png_file = svg_file.with_suffix('.png')
        
        # Determine dimensions
        width = height = None
        if size and len(size) >= 2:
            width, height = int(size[0]), int(size[1])
        elif scale != 1.0:
            # Default base size with scaling
            width = height = int(1024 * scale)
        
        # Convert SVG to PNG bytes using CairoSVG
        logger.info(f"Converting SVG {svg_path} to PNG with dimensions {width}x{height}")
        
        png_bytes = cairosvg.svg2png(
            url=str(svg_file),
            output_width=width,
            output_height=height,
            background_color=background if background else None
        )
        
        if png_bytes is None:
            raise ValueError(f"Failed to convert SVG to PNG: {svg_path}")
        
        # Load into PIL for post-processing
        img = Image.open(io.BytesIO(png_bytes))
        
        # Ensure RGBA format
        if img.mode != 'RGBA':
            img = img.convert('RGBA')
        
        # UMG optimizations
        arr = np.array(img, dtype=np.uint8)
        
        # Clean up fully transparent pixels (set RGB to 0 for better compression)
        transparent_mask = arr[..., 3] == 0
        arr[transparent_mask, 0:3] = 0
        
        # Premultiply alpha for UMG (better blending)
        rgb = arr[..., 0:3].astype(np.float32) / 255.0
        alpha = arr[..., 3:4].astype(np.float32) / 255.0
        rgb_premult = rgb * alpha
        arr[..., 0:3] = (rgb_premult * 255.0 + 0.5).astype(np.uint8)
        
        # Convert back to PIL and save
        final_img = Image.fromarray(arr, 'RGBA')
        
        # Ensure output directory exists
        png_file.parent.mkdir(parents=True, exist_ok=True)
        
        # Save with optimization
        final_img.save(png_file, format='PNG', optimize=True, compress_level=6)
        
        # Get final file info
        file_size = png_file.stat().st_size
        final_width, final_height = final_img.size
        
        logger.info(f"Successfully converted {svg_path} to {png_file} ({file_size} bytes)")
        
        return {
            "success": True,
            "png_path": str(png_file),
            "width": final_width,
            "height": final_height,
            "file_size": file_size,
            "method": "cairosvg_direct",
            "optimizations": ["premultiplied_alpha", "transparent_cleanup", "png_optimized"],
            "source_svg": str(svg_file)
        }
        
    except Exception as e:
        logger.error(f"Error converting SVG: {e}")
        return {"success": False, "error": str(e), "png_path": None}


def _handle_duplicate_asset(
    asset_path: str,
    destination_path: str,
    new_name: str
) -> Dict[str, Any]:
    """Handle asset duplication."""
    from vibe_ue_server import get_unreal_connection
    
    if not asset_path:
        return {"success": False, "error": "'asset_path' is required for duplicate action"}
    
    if not destination_path:
        return {"success": False, "error": "'destination_path' is required for duplicate action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Duplicating asset from {asset_path} to {destination_path}")
        response = unreal.send_command("duplicate_asset", {
            "asset_path": asset_path,
            "destination_path": destination_path,
            "new_name": new_name
        })
        
        if not response:
            return {"success": False, "error": "Failed to receive response from Unreal Engine"}
        
        return response
        
    except Exception as e:
        logger.error(f"Error duplicating asset: {e}")
        return {"success": False, "error": str(e)}


def _handle_save_asset(asset_path: str) -> Dict[str, Any]:
    """Handle saving a single asset."""
    from vibe_ue_server import get_unreal_connection
    
    if not asset_path:
        return {"success": False, "error": "'asset_path' is required for save action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Saving asset: {asset_path}")
        response = unreal.send_command("save_asset", {
            "asset_path": asset_path
        })
        
        if not response:
            return {"success": False, "error": "Failed to receive response from Unreal Engine"}
        
        return response
        
    except Exception as e:
        logger.error(f"Error saving asset: {e}")
        return {"success": False, "error": str(e)}


def _handle_save_all_assets(prompt_user: bool = False) -> Dict[str, Any]:
    """Handle saving all dirty assets."""
    from vibe_ue_server import get_unreal_connection
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Saving all dirty assets (prompt_user={prompt_user})")
        response = unreal.send_command("save_all_assets", {
            "prompt_user": prompt_user
        })
        
        if not response:
            return {"success": False, "error": "Failed to receive response from Unreal Engine"}
        
        return response
        
    except Exception as e:
        logger.error(f"Error saving all assets: {e}")
        return {"success": False, "error": str(e)}


def _handle_list_references(
    asset_path: str,
    include_dependencies: bool = False
) -> Dict[str, Any]:
    """Handle listing asset references (what references this asset and optionally what it depends on)."""
    from vibe_ue_server import get_unreal_connection
    
    if not asset_path:
        return {"success": False, "error": "'asset_path' is required for list_references action"}
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Listing references for asset: {asset_path} (include_dependencies={include_dependencies})")
        response = unreal.send_command("list_references", {
            "asset_path": asset_path,
            "include_dependencies": include_dependencies
        })
        
        if not response:
            return {"success": False, "error": "Failed to receive response from Unreal Engine"}
        
        return response
        
    except Exception as e:
        logger.error(f"Error listing references: {e}")
        return {"success": False, "error": str(e)}
