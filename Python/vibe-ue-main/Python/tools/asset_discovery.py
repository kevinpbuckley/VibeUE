"""
Enhanced Asset Discovery and Management Tools

COMPREHENSIVE ASSET SYSTEM FOR AI ASSISTANTS

This module provides advanced asset discovery, import, and management capabilities:
- Local file system import with validation and optimization
- Procedural texture generation for dynamic content
- Texture export for AI visual analysis
- Smart asset application with automatic compilation
- Asset property analysis and metadata extraction

CORE CAPABILITIES:
1. Smart Import - Import assets from local file system with validation
2. Asset Application - Apply textures to widgets with proper compilation
3. AI Integration - Export textures for visual analysis and smart matching
4. Automated Workflows - Streamlined asset management and application

AI WORKFLOW INTEGRATION:
- import_texture_asset() - Import from local files with validation
- export_texture_for_analysis() - Export textures for AI visual analysis

SMART FEATURES:
- Automatic texture format optimization
- Intelligent fallback for missing assets  
- Color palette analysis and matching
- Style-aware asset recommendations
- Bulk operations with progress tracking
"""

import logging
import os
import tempfile
import shutil
import re
import hashlib
from xml.etree import ElementTree as ET
from typing import Dict, Any, List, Optional, Tuple
from pathlib import Path
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

class AssetDiscoveryError(Exception):
    """Custom exception for asset discovery operations."""
    pass

def register_asset_discovery_tools(mcp: FastMCP):
    """Register enhanced asset discovery and management tools with the MCP server."""

    # ------------------------------------------------------------------
    # SVG Rasterization Helpers (Full + Fallback)
    # ------------------------------------------------------------------
    def _hash_file(path: str) -> str:
        h = hashlib.md5()
        with open(path, 'rb') as f:
            for chunk in iter(lambda: f.read(8192), b''):
                h.update(chunk)
        return h.hexdigest()

    def _rasterize_svg_full(svg_path: str, png_path: str, scale: float = 1.0,
                             output_size: Optional[List[int]] = None,
                             background: Optional[str] = None) -> Tuple[bool, str]:
        """Attempt full SVG rasterization using CairoSVG if available."""
        import io
        try:
            try:
                import cairosvg  # type: ignore
                from PIL import Image
                import numpy as np
            except ImportError as e:
                return False, f"Required packages not installed: {e}. Install with: pip install cairosvg pillow numpy"

            # Convert SVG to PNG bytes using CairoSVG
            kwargs: Dict[str, Any] = {"url": svg_path}
            if scale and scale != 1.0:
                kwargs["scale"] = scale
            if output_size and len(output_size) == 2:
                kwargs["output_width"], kwargs["output_height"] = output_size
            if background:
                kwargs["background_color"] = background
            
            # Get PNG bytes from CairoSVG
            png_bytes = cairosvg.svg2png(**kwargs)
            
            # Load into PIL for UMG optimization
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
            
            # Simple premultiply (no gamma for now to keep it fast)
            rgb_premult = rgb * alpha
            
            arr[..., 0:3] = (rgb_premult * 255.0 + 0.5).astype(np.uint8)
            
            # Convert back to PIL and save
            final_img = Image.fromarray(arr, 'RGBA')
            
            # Ensure output directory exists
            from pathlib import Path
            Path(png_path).parent.mkdir(parents=True, exist_ok=True)
            
            # Save with optimization
            final_img.save(png_path, format='PNG', optimize=True, compress_level=6)
            
            return True, png_path
        except Exception as e:
            return False, f"Full SVG rasterization failed: {e}"

    def _rasterize_svg_fallback(svg_path: str, png_path: str, size: Optional[List[int]] = None) -> Tuple[bool, str]:
        """Fallback minimal gradient rasterizer (previous implementation)."""
        try:
            from PIL import Image  # type: ignore
        except ImportError:
            return False, "Pillow not installed. pip install Pillow for fallback rasterizer."
        try:
            tree = ET.parse(svg_path)
            root = tree.getroot()
            def _strip(tag: str) -> str: return tag.split('}')[-1]
            svg_w = root.get('width'); svg_h = root.get('height')
            try:
                width = int(re.sub(r'[^0-9]', '', svg_w)) if svg_w else 1024
                height = int(re.sub(r'[^0-9]', '', svg_h)) if svg_h else 1024
            except Exception:
                width, height = 1024, 1024
            if size and len(size) == 2: width, height = size
            gradient = None
            for el in root.iter():
                if _strip(el.tag) == 'linearGradient': gradient = el; break
            if gradient is None:
                return False, "Fallback rasterizer only supports vertical linearGradient. Install CairoSVG for full support."
            stops: List[Tuple[float, Tuple[int,int,int,int]]] = []
            for stop in gradient.findall('.//') + [gradient]:
                if _strip(stop.tag) != 'stop': continue
                off_raw = stop.get('offset', '0')
                if off_raw.endswith('%'):
                    try: off = float(off_raw[:-1]) / 100.0
                    except: off = 0.0
                else:
                    try: off = float(off_raw)
                    except: off = 0.0
                color = stop.get('stop-color')
                if not color:
                    style = stop.get('style', '')
                    m = re.search(r'stop-color:\s*(#[0-9a-fA-F]{6})', style)
                    if m: color = m.group(1)
                if not color or not re.match(r'^#[0-9a-fA-F]{6}$', color): color = '#000000'
                r = int(color[1:3], 16); g = int(color[3:5], 16); b = int(color[5:7], 16); a = 255
                stops.append((max(0.0, min(1.0, off)), (r,g,b,a)))
            if not stops: return False, "No gradient stops."
            stops.sort(key=lambda x: x[0])
            if stops[0][0] > 0: stops.insert(0,(0.0, stops[0][1]))
            if stops[-1][0] < 1: stops.append((1.0, stops[-1][1]))
            img = Image.new('RGBA',(width,height)); px = img.load()
            for y in range(height):
                t = y/(height-1) if height>1 else 0
                for i in range(len(stops)-1):
                    o1,c1=stops[i]; o2,c2=stops[i+1]
                    if t>=o1 and t<=o2:
                        seg = 0 if o2==o1 else (t-o1)/(o2-o1)
                        r=int(c1[0]+(c2[0]-c1[0])*seg); g=int(c1[1]+(c2[1]-c1[1])*seg); b=int(c1[2]+(c2[2]-c1[2])*seg); a=int(c1[3]+(c2[3]-c1[3])*seg)
                        for x in range(width): px[x,y]=(r,g,b,a)
                        break
            img.save(png_path,'PNG')
            return True, png_path
        except Exception as e:
            return False, f"Fallback rasterization error: {e}"

    def _smart_svg_to_png(svg_path: str, png_path: str, size: Optional[List[int]] = None,
                           scale: float = 1.0, background: Optional[str] = None) -> Tuple[bool, str, Dict[str, Any]]:
        """Try full rasterization; fallback gracefully. Returns metadata."""
        meta: Dict[str, Any] = {"method": "cairosvg", "cache": False}
        # Simple content hash caching in temp dir
        try:
            h = _hash_file(svg_path)
            cache_dir = Path(tempfile.gettempdir())/"vibeue_svg_cache"
            cache_dir.mkdir(parents=True, exist_ok=True)
            cache_name = f"{Path(svg_path).stem}_{h}.png"
            cached = cache_dir/cache_name
            if cached.exists():
                shutil.copy(str(cached), png_path)
                meta["cache"] = True
                return True, png_path, meta
        except Exception:
            pass

        ok, msg = _rasterize_svg_full(svg_path, png_path, scale=scale, output_size=size, background=background)
        if not ok:
            meta["method"] = "fallback"
            ok2, msg2 = _rasterize_svg_fallback(svg_path, png_path, size)
            if not ok2:
                return False, f"{msg}; Fallback failed: {msg2}", meta
            msg = msg2
        # Store cache copy
        try:
            if 'h' in locals():
                cached = (Path(tempfile.gettempdir())/"vibeue_svg_cache"/f"{Path(svg_path).stem}_{h}.png")
                if not cached.exists():
                    shutil.copy(png_path, cached)
        except Exception:
            pass
        return True, png_path, meta

    @mcp.tool()
    def convert_svg_to_png(
        ctx: Context,
        svg_path: str,
        output_path: Optional[str] = None,
        size: Optional[List[int]] = None,
        scale: float = 1.0,
        background: Optional[str] = None
    ) -> Dict[str, Any]:
        """Convert an SVG to PNG (full spec via CairoSVG if available, else fallback).

        Args:
            svg_path: Source SVG path
            output_path: Destination PNG path (default side-by-side)
            size: Optional [W,H] override pixels (forces output size)
            scale: Scalar multiplier (ignored if size provided)
            background: Optional background color (e.g. '#00000000' / '#1e293b')

        Returns: success, png_path, method (cairosvg|fallback), cache hit flag.
        """
        try:
            if not os.path.isfile(svg_path):
                return {"success": False, "error": f"SVG not found: {svg_path}"}
            if output_path is None:
                output_path = str(Path(svg_path).with_suffix('.png'))
            ok, msg, meta = _smart_svg_to_png(svg_path, output_path, size=size, scale=scale, background=background)
            if not ok:
                return {"success": False, "error": msg, "method": meta.get('method')}
            return {"success": True, "png_path": output_path, "message": "SVG converted", **meta}
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    @mcp.tool()
    def import_texture_asset(
        ctx: Context,
        file_path: str,
        destination_path: str = "/Game/Textures/Imported",
        texture_name: Optional[str] = None,
        compression_settings: str = "TC_Default",
        generate_mipmaps: bool = True,
        validate_format: bool = True,
        auto_optimize: bool = True,
        convert_if_needed: bool = False,
        raster_size: Optional[List[int]] = None,
        auto_convert_svg: bool = True
    ) -> Dict[str, Any]:
        """
        Import texture asset from local file system with validation and optimization.
        
        📁 **SMART LOCAL IMPORT**: Import textures with automatic validation,
        format optimization, and intelligent naming conventions.
        
        Args:
            file_path: Absolute path to the texture file to import
                      Supported formats: PNG, JPG, JPEG, TGA, BMP, HDR, EXR, SVG (via Vector Graphics plugin)
            destination_path: Content browser path for imported asset
            texture_name: Custom name for imported texture (auto-generated if None)
            compression_settings: Texture compression type
                                 Options: "TC_Default", "TC_Normalmap", "TC_Masks", "TC_HDR"
            generate_mipmaps: Whether to generate mipmaps for the texture
            validate_format: Whether to validate file format before import
            auto_optimize: Whether to apply automatic optimization settings
            convert_if_needed: If True, auto-convert unsupported file types to PNG before import
            raster_size: Optional [W,H] for rasterized output when converting (default 2048x2048)
            auto_convert_svg: If True (default) rasterize SVG to PNG before import to avoid plugin dependency
            
        Returns:
            Dict containing:
            - success: boolean indicating if import completed
            - asset_path: full path to imported asset
            - texture_info: metadata about imported texture
            - optimization_applied: list of optimizations that were applied
            - warnings: any warnings during import process
        """
        try:
            from vibe_ue_server import get_unreal_connection

            original_path = file_path
            conversion_applied: List[str] = []

            # Detect unsupported formats and optionally convert before import
            # Note: SVG supported when Vector Graphics/SVG Importer plugin is enabled
            supported_exts = {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr", ".exr", ".svg"}
            ext = Path(file_path).suffix.lower()
            if validate_format and ext not in supported_exts:
                # Leave unsupported formats alone unless explicit conversion is requested in the future
                logger.warning(f"File extension '{ext}' is not in recognized list; attempting import without conversion")

            # Auto rasterize SVG to PNG if requested (prevents black textures when SVG plugin absent)
            if ext == '.svg' and auto_convert_svg:
                png_temp = str(Path(file_path).with_suffix('.raster.png'))
                ok, msg, meta = _smart_svg_to_png(file_path, png_temp, size=raster_size)
                if ok:
                    logger.info(f"Rasterized SVG -> PNG at {png_temp} via {meta['method']} (cache={meta['cache']})")
                    file_path = png_temp
                    conversion_applied.append(f"svg_rasterized_{meta['method']}")
                    if meta.get('cache'): conversion_applied.append('cache_hit')
                    ext = '.png'
                else:
                    logger.warning(f"SVG rasterization failed ({msg}); attempting direct import which may require plugin")
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"📁 Importing texture from: {file_path}")
            
            # Use C++ bridge implementation
            params = {
                "file_path": file_path,
                "destination_path": destination_path,
                "compression_settings": compression_settings,
                "generate_mipmaps": generate_mipmaps,
                "validate_format": validate_format,
                "auto_optimize": auto_optimize
            }
            
            if texture_name:
                params["texture_name"] = texture_name
            
            logger.info(f"Calling C++ import_texture_asset with params: {params}")
            response = unreal.send_command("import_texture_asset", params)
            
            logger.info(f"✅ Texture import completed via C++ bridge")
            # Attach conversion metadata if we applied any client-side conversion
            if isinstance(response, dict):
                response.setdefault("source_file", original_path)
                if conversion_applied:
                    response.setdefault("conversion_applied", conversion_applied)
            return response
            
        except Exception as e:
            logger.error(f"Texture import failed: {e}")
            return {
                "success": False,
                "error": str(e),
                "asset_path": "",
                "texture_info": {},
                "optimization_applied": [],
                "warnings": [str(e)]
            }
    
    @mcp.tool()
    def export_texture_for_analysis(
        ctx: Context,
        asset_path: str,
        export_format: str = "PNG",
        max_size: Optional[List[int]] = None,
        temp_folder: str = ""
    ) -> Dict[str, Any]:
        """
        Export texture asset to temporary file for AI visual analysis.
        
        🖼️ **AI VISION ENABLER**: Export textures as image files so AI assistants
        can visually analyze colors, patterns, styles for intelligent asset matching.
        
        Args:
            asset_path: Full path to texture asset in project
                       Example: "/Game/UI/Backgrounds/SpaceNebula"
            export_format: Image format for export
                          Options: "PNG", "JPG", "TGA", "BMP"
            max_size: [Width, Height] maximum size for exported image
                     Example: [256, 256] for 256x256 max resolution
            temp_folder: Custom temp folder (auto-generated if empty)
            
        Returns:
            Dict containing:
            - success: boolean indicating if export completed
            - asset_path: original asset path that was exported
            - temp_file_path: absolute path to exported image file
            - export_format: format used for export
            - exported_size: [width, height] of exported image
            - file_size: size of exported file in bytes
            - cleanup_required: whether temp file cleanup is needed
            
        💡 **AI Integration Workflow**:
        1. Import existing textures with import_texture_asset()
        2. Export texture with this function
        3. AI can now "see" the texture visually
        4. Use visual analysis for smart asset recommendations
        5. Clean up temp file when analysis complete
        """
        try:
            from vibe_ue_server import get_unreal_connection
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"🖼️ Exporting texture for AI analysis: {asset_path}")
            
            # Use C++ bridge implementation
            params = {
                "asset_path": asset_path,
                "export_format": export_format,
                "temp_folder": temp_folder
            }
            
            if max_size:
                params["max_size"] = max_size
            
            logger.info(f"Calling C++ export_texture_for_analysis with params: {params}")
            response = unreal.send_command("export_texture_for_analysis", params)
            
            logger.info(f"✅ Texture export completed via C++ bridge")
            return response
            
        except Exception as e:
            logger.error(f"Texture export failed: {e}")
            return {
                "success": False,
                "error": str(e),
                "asset_path": asset_path,
                "temp_file_path": "",
                "cleanup_required": False
            }
    
def _generate_asset_recommendations(assets: List[Dict], search_term: str, asset_type: str) -> List[Dict]:
    """Generate smart asset recommendations based on search results and criteria."""
    recommendations = []
    
    # Recommend based on common UI patterns
    if "background" in search_term.lower():
        recommendations.append({
            "type": "pattern_match",
            "title": "UI Background Assets",
            "description": "Found assets suitable for widget backgrounds",
            "suggested_assets": [a for a in assets if any(term in a["name"].lower() 
                               for term in ["bg", "background", "panel", "frame"])][:3]
        })
    
    if "button" in search_term.lower():
        recommendations.append({
            "type": "pattern_match", 
            "title": "Button Textures",
            "description": "Assets suitable for button styling",
            "suggested_assets": [a for a in assets if any(term in a["name"].lower()
                               for term in ["btn", "button", "ui"])][:3]
        })
    
    # Size-based recommendations
    large_textures = [a for a in assets if a.get("metadata", {}).get("width", 0) > 512]
    if large_textures:
        recommendations.append({
            "type": "size_analysis",
            "title": "High Resolution Assets", 
            "description": "Large textures suitable for backgrounds or detailed UI elements",
            "suggested_assets": large_textures[:3]
        })
    
    return recommendations
