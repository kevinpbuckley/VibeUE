# manage_asset - Test Prompt

## Purpose

This test validates asset management capabilities including searching for assets, importing assets, exporting assets, and inspecting asset properties through the VibeUE MCP tools.

## Prerequisites

- Unreal Engine is running with your project loaded
- VibeUE plugin is enabled
- MCP client is connected to the VibeUE server
- Connection verified with `check_unreal_connection`
- A few test image files available for import (PNG, JPG, or TGA)
- Write permissions to the project's Content folder

## Test Steps

### Test 1: Search for Blueprints

1. Ask your AI assistant: "Search for all blueprints in my project"

2. Review the list of blueprints returned

### Test 2: Search for Widgets

3. Ask your AI assistant: "Search for all widgets in my project"

4. Review the list of widget blueprints returned

### Test 3: Search for Specific Asset

5. Ask your AI assistant: "Search for assets named 'BP_TestActor'"

6. Verify the specific asset is found

### Test 4: Search with Filters

7. Ask your AI assistant: "Search for blueprints in the '/Game/Blueprints/' folder"

8. Review the filtered results

### Test 5: Get Asset Information (if supported)

9. Ask your AI assistant: "Get information about the asset at '/Game/Blueprints/BP_TestActor'"

10. Review the asset properties and metadata

### Test 6: Import Texture Asset

11. Ask your AI assistant: "Import the texture file 'test_image.png' from 'C:/path/to/test_image.png' to '/Game/Textures/test_image'"

12. Verify the texture was imported successfully

### Test 7: Search for Imported Texture

13. Ask your AI assistant: "Search for textures named 'test_image'"

14. Confirm the imported texture appears in search results

### Test 8: Export Texture for Analysis

15. Ask your AI assistant: "Export the texture '/Game/Textures/test_image' for analysis"

16. Verify the texture is exported and a temporary path is provided

### Test 9: Open Asset in Editor

17. Ask your AI assistant: "Open the asset '/Game/Blueprints/BP_TestActor' in the editor"

18. Verify the asset opens in Unreal Engine

### Test 10: Search for Materials (if any exist)

19. Ask your AI assistant: "Search for materials in my project"

20. Review any materials found

### Test 11: Search for Textures

21. Ask your AI assistant: "Search for textures in my project"

22. Review the list of textures

### Test 12: Search with Wildcards

23. Ask your AI assistant: "Search for assets starting with 'BP_'"

24. Verify wildcard search works

### Test 13: Error Handling - Invalid Path

25. Ask your AI assistant: "Get information about the asset at '/Game/NonExistent/FakeAsset'"

26. Verify appropriate error handling

### Test 14: Error Handling - Import Non-existent File

27. Ask your AI assistant: "Import the texture file 'nonexistent.png' from 'C:/path/to/nonexistent.png'"

28. Verify appropriate error message

## Expected Outcomes

### Test 1 - Search Blueprints
- ✅ Returns list of blueprints in the project
- ✅ Shows full asset paths
- ✅ Includes asset names
- ✅ May include asset types or parent classes
- ✅ List is formatted clearly

### Test 2 - Search Widgets
- ✅ Returns list of widget blueprints
- ✅ Shows full asset paths
- ✅ Distinguishes widgets from other blueprints
- ✅ May include widget root types

### Test 3 - Search Specific Asset
- ✅ Returns the specific asset if it exists
- ✅ Shows full asset path
- ✅ Returns empty or "not found" if asset doesn't exist
- ✅ Search is case-insensitive or handles case appropriately

### Test 4 - Search with Filters
- ✅ Returns only blueprints in specified folder
- ✅ Respects folder path filter
- ✅ Shows assets in subfolders if applicable
- ✅ Results match the filter criteria

### Test 5 - Get Asset Information
- ✅ Returns asset name and full path
- ✅ Shows asset type/class
- ✅ May include file size, modification date
- ✅ May include asset-specific properties

### Test 6 - Import Texture
- ✅ Texture is imported successfully
- ✅ Texture appears in Content Browser at specified path
- ✅ Confirmation message includes import path
- ✅ Texture maintains original image quality and properties
- ✅ Import handles path separators correctly (Windows/Unix)

### Test 7 - Search Imported Texture
- ✅ Imported texture appears in search results
- ✅ Shows full asset path
- ✅ Confirms texture is accessible

### Test 8 - Export Texture
- ✅ Texture is exported successfully
- ✅ Returns temporary file path or base64 data
- ✅ Exported file can be analyzed by AI
- ✅ Export preserves image quality
- ✅ Temporary file is accessible

### Test 9 - Open Asset
- ✅ Asset opens in the appropriate editor (Blueprint Editor)
- ✅ Confirmation message indicates asset was opened
- ✅ Editor focuses on the opened asset
- ✅ No errors during opening

### Test 10 - Search Materials
- ✅ Returns list of materials (if any exist)
- ✅ Shows full asset paths
- ✅ Returns empty list if no materials exist (with appropriate message)

### Test 11 - Search Textures
- ✅ Returns list of textures in project
- ✅ Includes imported textures
- ✅ Shows full asset paths
- ✅ May include texture dimensions or format

### Test 12 - Wildcard Search
- ✅ Returns all assets starting with "BP_"
- ✅ Wildcard matching works correctly
- ✅ May support other wildcard characters (*, ?)
- ✅ Results match the pattern

### Test 13 - Error Handling Invalid Path
- ✅ Clear error message indicating asset not found
- ✅ No crash or unexpected behavior
- ✅ Suggests using search to find assets
- ✅ Path validation provides helpful feedback

### Test 14 - Error Handling Import
- ✅ Clear error message indicating file not found
- ✅ No crash or unexpected behavior
- ✅ Suggests checking file path
- ✅ Import validation provides helpful feedback

## Notes

- Asset search uses Unreal's Asset Registry for fast lookups
- Full asset paths always start with '/Game/'
- Texture import supports PNG, JPG, TGA, BMP, EXR, HDR formats
- Exported textures are saved to temporary locations
- Asset paths use forward slashes (/) even on Windows
- Search may be case-insensitive depending on implementation
- Some asset types require the project to be saved after import
- Large imports may take time to process
- Always use exact asset paths from search results when possible
- Use `search_items` to find exact paths before operating on assets

## Cleanup

After testing:
- Delete imported test textures from Content Browser if desired
- Remove any exported temporary files
- Test assets (blueprints, widgets) can be kept or deleted
- Verify no temporary files remain in the project directory
