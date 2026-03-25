# manage_asset — save

Save a single asset to disk.

## Parameters

| Parameter    | Type   | Required | Description                           |
|--------------|--------|----------|---------------------------------------|
| `asset_path` | string | ✓        | Full Content Browser path to the asset |

## Notes

- Only saves the one specified asset.
- To save every modified asset in the project, use `save_all` instead.
- Always save after programmatic edits (Blueprints, StateTrees, Materials, etc.) to persist changes.

## Examples

```
manage_asset(action="save", asset_path="/Game/StateTree/ST_Cube")
manage_asset(action="save", asset_path="/Game/Blueprints/BP_Player")
```

## Response

```json
{ "success": true, "asset_path": "/Game/StateTree/ST_Cube" }
```
