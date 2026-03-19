# manage_asset — duplicate

Copy an asset to a new Content Browser location.

## Parameters

| Parameter          | Type   | Required | Description                              |
|--------------------|--------|----------|------------------------------------------|
| `source_path`      | string | ✓        | Path to the asset to copy                |
| `destination_path` | string | ✓        | Path for the new copy (must not exist)   |

## Notes

- The destination folder is created automatically if it does not exist.
- Use `find` to confirm the destination does not already exist before duplicating.
- After duplicating, the new asset is NOT automatically opened or saved — call `open` or `save` explicitly if needed.

## Examples

```
# Copy a Blueprint to make a variant
manage_asset(action="duplicate",
             source_path="/Game/Blueprints/BP_Enemy",
             destination_path="/Game/Blueprints/BP_EnemyBoss")

# Copy a StateTree as a starting point
manage_asset(action="duplicate",
             source_path="/Game/AI/ST_Patrol",
             destination_path="/Game/AI/ST_PatrolAdvanced")
```

## Response

```json
{
  "success": true,
  "source_path": "/Game/AI/ST_Patrol",
  "destination_path": "/Game/AI/ST_PatrolAdvanced"
}
```
