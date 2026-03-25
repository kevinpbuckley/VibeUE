# manage_asset — open

Open an asset in its native Unreal Editor (Blueprint Editor, Material Editor, StateTree Editor, etc.).

## Parameters

| Parameter    | Type   | Required | Description                           |
|--------------|--------|----------|---------------------------------------|
| `asset_path` | string | ✓        | Full Content Browser path to the asset |

## Notes

- The correct editor is chosen automatically based on asset type.
- If the asset is already open, its editor window is brought to focus.
- Use `search` or `find` first if you are not sure of the exact path.

## Examples

```
manage_asset(action="open", asset_path="/Game/StateTree/ST_Cube")
manage_asset(action="open", asset_path="/Game/Blueprints/BP_Player")
manage_asset(action="open", asset_path="/Game/Materials/M_Rock")
```

## Response

```json
{ "success": true, "asset_path": "/Game/StateTree/ST_Cube" }
```
