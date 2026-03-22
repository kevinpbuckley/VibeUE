# manage_asset

Multi-action tool for searching, locating, opening, saving, duplicating, moving, and deleting Unreal Engine assets.
Wraps `unreal.AssetDiscoveryService` — no Python code required.

## Actions

| Action      | Purpose                                              | Required params                         |
|-------------|------------------------------------------------------|-----------------------------------------|
| `search`    | Search by partial name and optional type             | `search_term`                           |
| `find`      | Look up a single asset by exact Content Browser path | `asset_path`                            |
| `list`      | List all assets under a folder (recursive)           | `path`                                  |
| `open`      | Open an asset in its native editor                   | `asset_path`                            |
| `save`      | Save a single asset to disk                          | `asset_path`                            |
| `save_all`  | Save all dirty/modified assets                       | _(none)_                                |
| `duplicate` | Copy an asset to a new location                      | `source_path`, `destination_path`       |
| `move`      | Move or rename an asset while preserving references  | `source_path`, `destination_path`       |
| `delete`    | Delete an asset (blocks if referenced)               | `asset_path`                            |
| `help`      | Show this overview or per-action detail              | `help_action` _(optional)_              |

## Path Format

All paths use the **Content Browser format** — never file-system paths.

```
/Game/Blueprints/BP_Player        ✓
/Game/AI/ST_Cube                  ✓
/Engine/BasicShapes/Cube          ✓
C:\Projects\Content\BP_Player.uasset   ✗
```

## Typical Workflow

```
1. manage_asset(action="search", search_term="BP_Cube", asset_type="Blueprint")
   → find the correct path

2. manage_asset(action="open", asset_path="/Game/Blueprints/BP_Cube")
   → open it in the editor

3. manage_asset(action="save", asset_path="/Game/Blueprints/BP_Cube")
   → save after edits
```

## Per-Action Help

```
manage_asset(action="help", help_action="search")
manage_asset(action="help", help_action="duplicate")
manage_asset(action="help", help_action="move")
manage_asset(action="help", help_action="delete")
```
