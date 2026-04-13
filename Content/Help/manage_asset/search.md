# manage_asset — search

Search for assets by partial name and optional asset type.

## Parameters

| Parameter     | Type   | Required | Description                                           |
|---------------|--------|----------|-------------------------------------------------------|
| `search_term` | string | ✓        | Partial name match (case-insensitive)                 |
| `asset_type`  | string |          | Class filter: Blueprint, StaticMesh, Texture2D, etc.  |

## Notes

- Searches all mounted content roots: `/Game/`, `/Engine/`, and all plugin content (e.g. `/VibeUE/`).
- Returns up to all matching assets — filter with `asset_type` to narrow results.
- Use the returned `object_path` or `package_name` as the `asset_path` for subsequent operations.

## Examples

```
# Find any asset with "Cube" in its name
manage_asset(action="search", search_term="Cube")

# Find only Blueprint assets matching "BP_Enemy"
manage_asset(action="search", search_term="BP_Enemy", asset_type="Blueprint")

# Find all StateTrees (empty search_term = all of this type)
manage_asset(action="search", search_term="", asset_type="StateTree")
```

## Response

```json
{
  "success": true,
  "count": 2,
  "search_term": "Cube",
  "asset_type": "Blueprint",
  "assets": [
    {
      "asset_name": "BP_Cube",
      "package_path": "/Game/Blueprints",
      "package_name": "/Game/Blueprints/BP_Cube",
      "asset_class": "Blueprint",
      "object_path": "/Game/Blueprints/BP_Cube.BP_Cube"
    }
  ]
}
```
