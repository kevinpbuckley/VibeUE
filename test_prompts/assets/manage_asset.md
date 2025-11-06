# manage_asset Test Prompts

## Prerequisites
- ✅ Unreal Engine 5.6+ running
- ✅ VibeUE plugin loaded
- ✅ MCP connection active
- ✅ Test assets available in project

## Setup: Create Test Assets

**Run these commands BEFORE starting tests:**

```
Create Blueprint "BP_AssetSearchTest" with parent "Actor"
Create Blueprint "WBP_AssetSearchTest" with parent "UserWidget"
```

## Overview
Tests all 6 actions of `manage_asset` covering search, texture import/export, editor operations, SVG conversion, and asset duplication.

## Test 1: Asset Search

**Purpose**: Search for various asset types in project

### Steps

1. **Search for Widgets**
   ```
   Search for assets with search_term="radar", asset_type="Widget"
   ```

2. **Search for Textures**
   ```
   Search for assets with search_term="background", asset_type="Texture2D"
   ```

3. **Search for Blueprints**
   ```
   Search all Blueprints with asset_type="Blueprint"
   ```

4. **Search in Specific Path**
   ```
   Search in path="/Game/UI" for widget assets
   ```

5. **Case-Sensitive Search**
   ```
   Test search with case_sensitive=true vs false
   ```

### Expected Outcomes
- ✅ search returns matching assets
- ✅ asset_type filters by type
- ✅ path parameter limits search scope
- ✅ case_sensitive affects results
- ✅ Returns package_path for each asset

---

## Test 2: Search with Advanced Options

**Purpose**: Test search filtering options

### Steps

1. **Include Engine Content**
   ```
   Search with include_engine_content=true
   ```

2. **Limit Results**
   ```
   Search with max_results=10
   ```

3. **Search All Asset Types**
   ```
   Search without asset_type to see all matching assets
   ```

### Expected Outcomes
- ✅ include_engine_content shows engine assets
- ✅ max_results limits returned items
- ✅ Omitting asset_type searches all types
- ✅ Performance acceptable with large result sets

---

## Test 3: Texture Import

**Purpose**: Import texture from file system

### Steps

1. **Prepare Source File**
   ```
   Have a test PNG/JPG file ready on disk
   Example: C:/Temp/test_texture.png
   ```

2. **Import Basic**
   ```
   Import texture:
   - file_path: "C:/Temp/test_texture.png"
   - destination_path: "/Game/Textures/Imported"
   - texture_name: "T_TestImport"
   ```

3. **Import with Compression**
   ```
   Import with compression_settings="TC_Default"
   ```

4. **Import with Auto-Optimization**
   ```
   Import with auto_optimize=true
   ```

5. **Verify Import**
   ```
   Search for T_TestImport to confirm it exists
   ```

### Expected Outcomes
- ✅ Texture imported to specified location
- ✅ texture_name applied correctly
- ✅ Compression settings respected
- ✅ auto_optimize processes texture
- ✅ Searchable after import

---

## Test 4: Texture Export

**Purpose**: Export texture for AI analysis

### Steps

1. **Export Existing Texture**
   ```
   Export texture:
   - asset_path: "/Game/Textures/T_Background"
   - export_format: "PNG"
   - max_size: [256, 256]
   ```

2. **Export Full Resolution**
   ```
   Export without max_size to get full resolution
   ```

3. **Verify Export Path**
   ```
   Check returned temp file path exists
   ```

4. **AI Vision Analysis**
   ```
   Use returned path for AI vision analysis (external to VibeUE)
   ```

### Expected Outcomes
- ✅ Texture exported to temp location
- ✅ export_format determines file type
- ✅ max_size limits resolution
- ✅ Returns temp file path
- ✅ File accessible for AI processing

### Use Case
Export texture → Analyze with AI vision → Provide feedback → Reimport modified version

---

## Test 5: Open in Editor

**Purpose**: Open assets in appropriate editors

### Steps

1. **Open Texture**
   ```
   Open texture asset in texture editor
   ```

2. **Open Blueprint**
   ```
   Open Blueprint asset in Blueprint editor
   ```

3. **Open Material**
   ```
   Open Material in Material editor
   ```

4. **Test Force Open**
   ```
   Open already-open asset with force_open=true
   ```

### Expected Outcomes
- ✅ Texture opens in texture editor
- ✅ Blueprint opens in Blueprint editor
- ✅ Material opens in Material editor
- ✅ force_open brings asset to front if already open
- ✅ Correct editor type for each asset

---

## Test 6: SVG to PNG Conversion

**Purpose**: Convert SVG files to PNG for use as textures

### Steps

1. **Convert Basic SVG**
   ```
   Convert SVG to PNG:
   - svg_path: "C:/Icons/test_icon.svg"
   - output_path: "C:/Temp/test_icon.png"
   - size: [512, 512]
   ```

2. **Convert with Background**
   ```
   Convert with background="#FFFFFF" for white background
   ```

3. **Convert with Transparency**
   ```
   Convert with background="#00000000" for transparent
   ```

4. **Convert with Scale**
   ```
   Use scale parameter instead of size
   ```

5. **Import Converted PNG**
   ```
   Use import_texture to bring PNG into Unreal
   ```

### Expected Outcomes
- ✅ SVG converted to PNG
- ✅ size parameter controls output dimensions
- ✅ background sets PNG background color
- ✅ Transparency preserved
- ✅ Converted PNG importable to Unreal

---

## Test 7: Complete Texture Workflow

**Purpose**: End-to-end texture import, modification, export cycle

### Steps

1. **Search for Existing Texture**
   ```
   Find texture to modify
   ```

2. **Export for Editing**
   ```
   Export texture to temp location
   ```

3. **Simulate External Edit**
   ```
   (External tool would modify file here)
   ```

4. **Import Modified Version**
   ```
   Import modified texture back to project
   ```

5. **Open in Editor**
   ```
   Verify changes in texture editor
   ```

### Expected Outcomes
- ✅ Complete roundtrip workflow functional
- ✅ Textures maintain quality through export/import
- ✅ Workflow supports AI-assisted texture editing
- ✅ Asset versioning possible

---

## Test 8: SVG Icon Workflow

**Purpose**: Import UI icons from SVG sources

### Steps

1. **Prepare SVG Icons**
   ```
   Have set of SVG UI icons ready
   ```

2. **Convert to PNG at Multiple Sizes**
   ```
   Convert same SVG to:
   - 64x64 for small icons
   - 128x128 for medium
   - 512x512 for high-res
   ```

3. **Import All Sizes**
   ```
   Import each PNG with descriptive names:
   - T_Icon_Play_64
   - T_Icon_Play_128
   - T_Icon_Play_512
   ```

4. **Search to Verify**
   ```
   Search for imported icons
   ```

### Expected Outcomes
- ✅ Multiple size variants created
- ✅ All icons imported successfully
- ✅ Naming convention maintained
- ✅ Icons usable in UMG widgets

---

## Advanced Search Patterns

### Find All UMG Widgets
```
search(asset_type="Widget", path="/Game")
```

### Find Textures in Specific Folder
```
search(search_term="", asset_type="Texture2D", path="/Game/Textures/UI")
```

### Find Blueprints by Name Pattern
```
search(search_term="BP_Enemy", asset_type="Blueprint")
```

### Performance-Aware Search
```
search(search_term="specific_name", max_results=50)
# Better than broad search with no filters
```

---

## Import/Export Best Practices

### Texture Import
- **Use auto_optimize**: Let engine optimize textures
- **Set compression**: TC_Default for most cases
- **Name consistently**: T_ prefix for textures
- **Organize folders**: Keep textures in /Game/Textures/

### Texture Export
- **Limit resolution**: Use max_size for AI analysis (faster processing)
- **Choose format**: PNG for transparency, JPG for photos
- **Cleanup**: Delete temp files after use

### SVG Conversion
- **Size appropriately**: Match expected texture resolution
- **Test transparency**: Verify alpha channel handling
- **Batch convert**: Convert multiple sizes at once
- **Background color**: Use #00000000 for transparent, #FFFFFF for white

---

## Test 9: Asset Duplication

**Purpose**: Duplicate existing assets to new locations

### Steps

1. **Duplicate Blueprint**
   ```
   Duplicate asset:
   - asset_path: "/Game/Blueprints/BP_Player"
   - destination_path: "/Game/Blueprints/Characters"
   - new_name: "BP_Player2"
   ```

2. **Duplicate Widget Blueprint**
   ```
   Duplicate asset:
   - asset_path: "/Game/UI/WBP_MainMenu"
   - destination_path: "/Game/UI/Menus"
   - new_name: "WBP_PauseMenu"
   ```

3. **Duplicate Material**
   ```
   Duplicate asset:
   - asset_path: "/Game/Materials/M_Base"
   - destination_path: "/Game/Materials/Variants"
   - new_name: "M_Base_Red"
   ```

4. **Duplicate Without New Name**
   ```
   Duplicate with auto-generated name (omit new_name parameter)
   ```

5. **Verify Duplicated Asset**
   ```
   Search for duplicated asset to confirm it exists
   Open duplicated asset in editor to verify it's correct
   ```

### Expected Outcomes
- ✅ Asset duplicated to specified location
- ✅ new_name applied correctly
- ✅ Auto-generated name when new_name omitted
- ✅ Returns original_path, new_path, and asset_type
- ✅ Duplicated asset maintains original functionality
- ✅ Works with different asset types (Blueprints, Widgets, Materials, Textures)

### Error Cases to Test
- ❌ Source asset doesn't exist → ASSET_NOT_FOUND error
- ❌ Destination path invalid → INVALID_PATH error
- ❌ Name conflict at destination → handled by Unreal with suffix

---

## Reference: All Actions Summary

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| **search** | Find assets | search_term, asset_type, path, case_sensitive, include_engine_content, max_results |
| **import_texture** | Import from file | file_path, destination_path, texture_name, compression_settings, auto_optimize |
| **export_texture** | Export for analysis | asset_path, export_format, max_size |
| **open_in_editor** | Open asset | asset_path, force_open |
| **svg_to_png** | Convert SVG | svg_path, output_path, size, scale, background |
| **duplicate** | Duplicate asset | asset_path, destination_path, new_name |

---

## Cleanup: Delete Test Assets

**Run these commands AFTER completing all tests:**

```
Delete all test assets:
- Delete /Game/Blueprints/BP_AssetSearchTest with force_delete=True and show_confirmation=False
- Delete /Game/Blueprints/WBP_AssetSearchTest with force_delete=True and show_confirmation=False
```

**For automated testing:** Pass `force_delete=True` and `show_confirmation=False` as **direct parameters** (not in extra dict) to enable instant deletion without confirmation dialogs.

---

**Test Coverage**: 6/6 actions tested ✅  
**Last Updated**: November 6, 2025  
**Related Issues**: #69, #76, #91

