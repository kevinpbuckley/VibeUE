# Asset Discovery & Management

## Overview

Comprehensive guide for finding, importing, managing, and working with Unreal Engine assets through VibeUE MCP.

## Asset Search

### Universal Search Tool

**`search_items()` - Find any asset type:**

```python
# Find widgets
search_items(search_term="Inventory", asset_type="Widget")

# Find textures
search_items(search_term="background", asset_type="Texture2D")

# Find all materials
search_items(asset_type="Material")

# Search specific folder
search_items(path="/Game/UI")

# Combined filters
search_items(
    search_term="Button",
    asset_type="Widget",
    path="/Game/UI/Components"
)
```

### Supported Asset Types

**Common Types:**
- `Widget`, `WidgetBlueprint` - UMG widgets
- `Blueprint` - Blueprint classes
- `Texture2D` - Textures
- `Material`, `MaterialInstance` - Materials
- `StaticMesh` - Static meshes
- `SkeletalMesh` - Skeletal meshes
- `SoundCue`, `SoundWave` - Audio assets
- `NiagaraSystem` - Particle systems
- `DataTable` - Data tables

### Search Parameters

```python
search_items(
    search_term="",              # Text filter (optional)
    asset_type="",               # Asset type filter (optional)
    path="/Game",                # Search path (default /Game)
    case_sensitive=False,        # Case sensitivity (default False)
    include_engine_content=False,# Include engine assets (default False)
    max_results=100              # Result limit (default 100)
)
```

### Using Search Results

**CRITICAL: Use `package_path` field, NOT `path` with duplicated name!**

```python
# ✅ CORRECT - Use package_path
search_result = search_items(search_term="Inventory", asset_type="Widget")
widget_path = search_result["items"][0]["package_path"]  # "/Game/UI/WBP_Inventory"
get_widget_blueprint_info(widget_path)  # Fast, direct load

# ❌ WRONG - Don't use path field with duplicated name
widget_path = search_result["items"][0]["path"]  # "/Game/UI/WBP_Inventory.WBP_Inventory"
get_widget_blueprint_info(widget_path)  # Can timeout!
```

### Search Result Structure

```json
{
  "success": true,
  "items": [
    {
      "name": "WBP_Inventory",
      "path": "/Game/UI/WBP_Inventory.WBP_Inventory",  // ❌ Avoid this
      "package_path": "/Game/UI/WBP_Inventory",        // ✅ Use this
      "type": "WidgetBlueprint",
      "asset_class": "UWidgetBlueprint"
    }
  ],
  "search_info": {
    "search_term": "Inventory",
    "asset_type": "Widget",
    "path": "/Game",
    "count": 1
  }
}
```

## Blueprint Discovery

### Get Blueprint Information

```python
# Universal Blueprint inspector
get_blueprint_info("/Game/Blueprints/BP_Player")

# Returns:
# - Variables (name, type, type_path)
# - Components (mesh, camera, lights)
# - Widget Components (for widget blueprints)
# - Functions
# - Event graphs
# - Properties
```

### Performance Tips

**Fast Paths (50ms):**
- ✅ Package paths: `/Game/Blueprints/BP_Player`
- ✅ From search_items package_path field

**Timeout Risk (5-60+ seconds):**
- ❌ Duplicated object paths: `/Game/UI/Widget.Widget`
- ❌ Partial names without search: `BP_Player`

**Best Practice:**
```python
# 1. Always search first
result = search_items(search_term="Player", asset_type="Blueprint")
bp_path = result["items"][0]["package_path"]

# 2. Use package path
get_blueprint_info(bp_path)
```

## Texture Asset Management

### Import Textures

```python
# Import texture from file system
import_texture_asset(
    file_path="C:/Path/To/texture.png",
    destination_path="/Game/Textures/UI",
    texture_name="T_Button",
    compression_settings="TC_Default",
    generate_mipmaps=True,
    auto_optimize=True,
    convert_if_needed=False,  # Auto-convert unsupported formats
    auto_convert_svg=True     # Rasterize SVG to PNG (default)
)

# Supported formats: PNG, JPG, JPEG, TGA, BMP, HDR, EXR, SVG
```

### Export Textures for Analysis

```python
# Export texture for AI visual analysis
export_texture_for_analysis(
    asset_path="/Game/Textures/UI/T_Button",
    export_format="PNG",
    max_size=[256, 256]  # Scale for preview
)

# Use for:
# - AI visual analysis of textures
# - Color palette extraction
# - Style matching
# - Asset recommendations
```

### Texture Compression Settings

**Common Settings:**
- `TC_Default` - Standard textures
- `TC_Normalmap` - Normal maps
- `TC_Masks` - Masks and UI
- `TC_HDR` - HDR textures

## Asset Opening in Editor

**Use `manage_asset` with `action="open_in_editor"` to open ANY asset in its appropriate editor:**

```python
# Open asset in appropriate editor (works for materials, blueprints, widgets, textures, etc.)
manage_asset(action="open_in_editor", asset_path="/Game/Textures/UI/T_ButtonNormal")
manage_asset(action="open_in_editor", asset_path="/Game/Materials/M_MetalShiny")
manage_asset(action="open_in_editor", asset_path="/Game/UI/WBP_MainMenu")
manage_asset(action="open_in_editor", asset_path="/Game/Blueprints/BP_Player")

# With force_open parameter
manage_asset(
    action="open_in_editor",
    asset_path="/Game/Materials/M_Metal",
    force_open=False  # False = focus existing, True = force reopen
)
```

**IMPORTANT:** Do NOT use `manage_material(action="open_editor")` - that action does not exist! Always use `manage_asset` to open assets in their editors.

## Asset Type Detection

### By Naming Convention

**Common Prefixes:**
- `T_` - Textures (T_Button, T_Background)
- `M_` - Materials (M_Metal, M_Glass)
- `MI_` - Material Instances (MI_CharacterSkin)
- `SM_` - Static Meshes (SM_Rock, SM_Building)
- `BP_` - Blueprints (BP_Player, BP_Enemy)
- `WBP_` - Widget Blueprints (WBP_MainMenu, WBP_HUD)
- `A_` - Audio (A_Explosion, A_Music)
- `NS_` - Niagara Systems (NS_Fire, NS_Smoke)

### By Asset Class

```python
# Search results include asset_class
result = search_items(search_term="Button")
for item in result["items"]:
    print(f"{item['name']} is {item['asset_class']}")
```

## Common Discovery Workflows

### Find and Modify Widget

```python
# 1. Search
widgets = search_items(search_term="Inventory", asset_type="Widget")

# 2. Get package path (CRITICAL)
widget_path = widgets["items"][0]["package_path"]

# 3. Inspect
info = get_widget_blueprint_info(widget_path)

# 4. List components
components = list_widget_components(widget_path)

# 5. Modify
set_widget_property(widget_path, "Title_Text", "ColorAndOpacity", [0, 1, 1, 1])
```

### Find and Style with Texture

```python
# 1. Find texture
textures = search_items(search_term="background", asset_type="Texture2D", path="/Game/Textures")

# 2. Find widget
widgets = search_items(search_term="Menu", asset_type="Widget")

# 3. Apply texture
widget_path = widgets["items"][0]["package_path"]
texture_path = textures["items"][0]["package_path"]

set_widget_property(
    widget_path,
    "Background_Image",
    "Brush.Texture",
    texture_path
)
```

### Import and Apply Texture

```python
# 1. Import texture
import_result = import_texture_asset(
    file_path="C:/Downloads/sci_fi_background.png",
    destination_path="/Game/Textures/UI",
    texture_name="T_SciFiBackground"
)

# 2. Find widget
widgets = search_items(search_term="MainMenu", asset_type="Widget")
widget_path = widgets["items"][0]["package_path"]

# 3. Apply texture
set_widget_property(
    widget_path,
    "Background_Image",
    "Brush.Texture",
    import_result["asset_path"]
)
```

## Error Recovery

### Asset Not Found

```python
# ❌ Error: "Asset not found"
# ✅ Solution:
search_items(search_term="partial_name")  # Find exact name
```

### Timeout on Blueprint Info

```python
# ❌ Cause: Using duplicated object path
get_blueprint_info("/Game/UI/Widget.Widget")

# ✅ Solution: Use package path
search_result = search_items(search_term="Widget", asset_type="Widget")
get_blueprint_info(search_result["items"][0]["package_path"])
```

### No Results from Search

```python
# Try broader search
search_items(search_term="", path="/Game/UI")  # All assets in folder

# Try case-insensitive
search_items(search_term="inventory", case_sensitive=False)

# Try without type filter
search_items(search_term="inventory")  # All types
```

## Best Practices

1. **Always Search First**: Never hardcode asset names
2. **Use Package Paths**: Avoid duplicated object paths from `path` field
3. **Cache Results**: Store search results to minimize redundant calls
4. **Validate Paths**: Check asset_path in results before using
5. **Filter Smartly**: Use path parameter to narrow search scope
6. **Handle Not Found**: Always check success field in responses

## Pro Tips

- **Empty Search Term**: `search_items(search_term="", path="/Game/UI")` lists all assets in folder
- **Type Discovery**: Search without filters to see what types exist
- **Path Exploration**: Search incrementally: `/Game` → `/Game/UI` → `/Game/UI/Components`
- **Engine Assets**: Set `include_engine_content=True` to access engine materials/textures
- **Max Results**: Increase for comprehensive searches, decrease for quick lookups
