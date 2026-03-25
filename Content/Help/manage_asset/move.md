# manage_asset — move

Move or rename an asset while preserving references.

## Parameters

| Parameter          | Type   | Required | Description                                 |
|--------------------|--------|----------|---------------------------------------------|
| `source_path`      | string | ✓        | Path to the existing asset                  |
| `destination_path` | string | ✓        | New path for the asset (must not exist)     |

## Notes

- `move` handles both folder moves and renames.
- Use `move` instead of duplicate + delete. Duplicate creates a new asset identity and can break references when the original is deleted.
- The destination folder is created automatically by Unreal when the move succeeds.

## Examples

```
# Move a StateTree task into a subfolder
manage_asset(action="move",
             source_path="/Game/StateTree/STT_Rotate",
             destination_path="/Game/StateTree/Tasks/STT_Rotate")

# Rename a Blueprint in place
manage_asset(action="move",
             source_path="/Game/Blueprints/BP_Enemy",
             destination_path="/Game/Blueprints/BP_EnemyBoss")
```

## Response

```json
{
  "success": true,
  "source_path": "/Game/StateTree/STT_Rotate",
  "destination_path": "/Game/StateTree/Tasks/STT_Rotate"
}
```