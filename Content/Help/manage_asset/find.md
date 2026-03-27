# manage_asset — find

Look up a single asset by its exact Content Browser path.

## Parameters

| Parameter    | Type   | Required | Description                                  |
|--------------|--------|----------|----------------------------------------------|
| `asset_path` | string | ✓        | Full Content Browser path, no extension      |

## Notes

- Use this to confirm whether an asset exists before editing it.
- The path must be a Content Browser path: `/Game/...` or `/Engine/...`.
- Do NOT include the asset extension (`.uasset`) or object suffix (`.BP_Cube`).

## Examples

```
manage_asset(action="find", asset_path="/Game/StateTree/ST_Cube")
manage_asset(action="find", asset_path="/Game/Blueprints/BP_Player")
```

## Response (found)

```json
{
  "success": true,
  "found": true,
  "asset": {
    "asset_name": "ST_Cube",
    "package_path": "/Game/StateTree",
    "package_name": "/Game/StateTree/ST_Cube",
    "asset_class": "StateTree",
    "object_path": "/Game/StateTree/ST_Cube.ST_Cube"
  }
}
```

## Response (not found)

```json
{
  "success": true,
  "found": false,
  "message": "No asset found at path: /Game/StateTree/ST_Missing"
}
```
