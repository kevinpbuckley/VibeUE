# manage_asset — list

List all assets under a Content Browser folder recursively.

## Parameters

| Parameter    | Type   | Required | Description                                         |
|--------------|--------|----------|-----------------------------------------------------|
| `path`       | string | ✓        | Content Browser folder: `/Game/AI`, `/Engine`, etc. |
| `asset_type` | string |          | Optional class filter: Blueprint, StaticMesh, etc.  |

## Notes

- Results include all assets within `path` and all sub-folders (recursive).
- Use `asset_type` to avoid large result sets in broad folders like `/Game`.
- `/Engine` is valid and lists built-in engine assets (meshes, materials, etc.).

## Examples

```
# All assets in /Game/AI folder
manage_asset(action="list", path="/Game/AI")

# Only Blueprints under /Game
manage_asset(action="list", path="/Game", asset_type="Blueprint")

# Engine static meshes (built-in shapes like Cube, Sphere)
manage_asset(action="list", path="/Engine", asset_type="StaticMesh")
```

## Response

```json
{
  "success": true,
  "count": 3,
  "path": "/Game/AI",
  "asset_type": "",
  "assets": [
    {
      "asset_name": "ST_Cube",
      "package_path": "/Game/AI",
      "package_name": "/Game/AI/ST_Cube",
      "asset_class": "StateTree",
      "object_path": "/Game/AI/ST_Cube.ST_Cube"
    }
  ]
}
```
