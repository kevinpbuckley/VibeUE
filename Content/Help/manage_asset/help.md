# manage_asset

Manage assets in the Unreal Engine project - search, import, export, save, delete, and manipulate project assets.

## Summary

The `manage_asset` tool provides comprehensive asset management capabilities for your Unreal Engine project. It allows you to search for assets by name or type, import and export textures, duplicate existing assets, manage asset references, and perform save operations.

## Actions

| Action | Description |
|--------|-------------|
| search | Search for assets by name, path, or class type |
| import_texture | Import a texture file into the project |
| export_texture | Export a texture asset to an external file |
| delete | Delete an asset from the project |
| duplicate | Create a copy of an existing asset |
| save | Save a specific asset |
| save_all | Save all modified assets in the project |
| list_references | List all assets that reference or are referenced by an asset |
| open | Open an asset in its appropriate editor |

## Usage

### Search for Assets
```json
{
  "Action": "search",
  "ParamsJson": "{\"SearchPattern\": \"*Character*\", \"ClassFilter\": \"Blueprint\"}"
}
```

### Import a Texture
```json
{
  "Action": "import_texture",
  "ParamsJson": "{\"SourcePath\": \"C:/Textures/hero_diffuse.png\", \"DestinationPath\": \"/Game/Textures/Characters\"}"
}
```

### Save All Assets
```json
{
  "Action": "save_all",
  "ParamsJson": "{}"
}
```

## Notes

- Asset paths should use the `/Game/` prefix for content folder assets
- When deleting assets, ensure no other assets reference them to avoid broken references
- Import operations support common image formats (PNG, TGA, JPG, etc.)
- Use `list_references` before deleting to check for dependencies

## Common Patterns

### Finding Assets
```python
# Search for all Blueprints
manage_asset(action="search", search_term="BP_", asset_type="Blueprint")

# Find textures
manage_asset(action="search", search_term="", asset_type="Texture2D", path="/Game/Textures")
```

### Opening Assets in Editor
```python
# Use manage_asset to open ANY asset type in its editor
manage_asset(action="open_in_editor", asset_path="/Game/Materials/M_MyMaterial")
manage_asset(action="open_in_editor", asset_path="/Game/Blueprints/BP_MyActor")
manage_asset(action="open_in_editor", asset_path="/Game/UI/WBP_MainMenu")
```

### Saving Work
```python
# Always save after making changes
manage_asset(action="save_all")  # Save all dirty assets
```

## Common Mistakes to Avoid

**Opening Assets:**
Many tools do NOT have their own "open_editor" action. Use `manage_asset` instead:
- ❌ WRONG: `manage_material(action="open_editor", ...)` - doesn't exist!
- ❌ WRONG: `manage_blueprint(action="open", ...)` - may not exist!
- ✅ CORRECT: `manage_asset(action="open_in_editor", asset_path="...")` - works for ALL assets

**Asset Path Format:**
- ✅ Use full paths: `/Game/Blueprints/BP_MyActor`
- ❌ Don't use partial names: `BP_MyActor`
