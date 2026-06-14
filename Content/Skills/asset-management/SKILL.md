---
name: asset-management
display_name: Asset Discovery & Management
description: Search, find, open, move, duplicate, save, delete, import, and export assets (the manage_asset tool / AssetDiscoveryService). Use when the user asks to find/locate an asset, list a folder, open/save/duplicate/move/rename/delete an asset, import an image from disk, list assets open in editors, or query/use the Content Browser selection. Prefer the manage_asset MCP tool over raw Python for asset ops.
vibeue_classes:
  - AssetDiscoveryService
unreal_classes:
  - EditorAssetLibrary
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "asset-management"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Asset Discovery & Management Skill

## Critical Rules

### ⚠️ Out-Params Become Return Values in Python — Never Pass an `AssetData` Argument

C++ methods shaped like `bool Func(FAssetData& Out)` are exposed to Python as
`func(...) -> AssetData or None`. Passing an `AssetData` argument raises `TypeError`.

```python
# WRONG - TypeError: takes no arguments (1 given)
asset = unreal.AssetData()
unreal.AssetDiscoveryService.get_primary_content_browser_selection(asset)

# CORRECT - call with no out-arg, check the return for None
asset = unreal.AssetDiscoveryService.get_primary_content_browser_selection()
if asset:
    print(asset.asset_name)
```

Affected methods (all return `AssetData or None`):

| Method | Python signature |
|--------|------------------|
| `find_asset_by_path` | `(asset_path) -> AssetData or None` |
| `get_active_asset` | `() -> AssetData or None` |
| `get_primary_content_browser_selection` | `() -> AssetData or None` |

### 🚨 Never list broad paths — a `/Game` listing can return 30,000+ assets

`manage_asset action='list' path='/Game'` (and `list_assets_in_path("/Game", "")`) returns
**every asset under the path** — tens of thousands of entries in a real project, flooding the
conversation. Always narrow first:

- Filter by type AND term: `manage_asset action='search' search_term='WBP_' asset_type='WidgetBlueprint'`
- Or list a **specific subfolder**: `list_assets_in_path("/Game/Blueprints/HUD", "WidgetBlueprint")`
- If you only need a count or sample, slice in Python before printing — never print the full array.

### ⚠️ `search_assets` Searches ALL Mounted Roots — Game, Engine, and Plugins

`search_assets(term, type)` searches `/Game/`, `/Engine/`, and every mounted plugin in one pass.
There is NO third parameter.

```python
# WRONG - will error (only 2 params exist)
assets = unreal.AssetDiscoveryService.search_assets("Cube", "", True)

# CORRECT - results include /Engine and plugin content; filter by package_path if you
# only want project content
hits = unreal.AssetDiscoveryService.search_assets("Cube", "StaticMesh")
game_only = [a for a in hits if str(a.package_path).startswith("/Game")]

# To scope to one folder, use list_assets_in_path instead
engine_assets = unreal.AssetDiscoveryService.list_assets_in_path("/Engine", "Texture2D")
```

### ⚠️ `EditorAssetLibrary` Save Methods

There is **no `save_dirty_assets`** (`AttributeError`). The real options:
`EditorAssetLibrary.save_asset(path)`, `save_directory(dir)`, `save_loaded_asset(obj)`,
`save_loaded_assets([objs])` — or simplest, the `manage_asset` tool with `action='save_all'`
(saves every dirty asset).

### ⚠️ `EditorAssetLibrary.list_assets` Has No Class Filter

`unreal.EditorAssetLibrary.list_assets(directory_path, recursive=True, include_folder=False)`
returns **path strings only** — there is no `asset_class_names` or any other class-filter kwarg
(`TypeError: invalid keyword argument`). To list assets of one type in a folder, use
`AssetDiscoveryService.list_assets_in_path("/Game/Input", "InputAction")` instead.

### ⚠️ UE 5.7 Property Changes

| WRONG (old) | CORRECT (UE 5.7) |
|-------------|------------------|
| `asset.name` | `asset.asset_name` |
| `asset.path` | `asset.package_path` |
| `asset.asset_class` | `asset.asset_class_path` |
| `asset.object_path` | `f"{asset.package_name}.{asset.asset_name}"` |

`AssetData` has **no** `object_path` attribute in 5.7 (`AttributeError`). Build it from
`package_name` + `asset_name`, or just use `str(asset.package_name)` — every
AssetDiscoveryService method that takes a path accepts the package name form.

### ⚠️ Importing Image Files From Disk — Use the Built-in Importer

To bring an image file from disk into the Content Browser, use the **`manage_asset` import action**
(or `AssetDiscoveryService.import_asset`). Do **NOT** call `unreal.AssetToolsHelpers...import_asset_tasks`
or `ImportAssets` from `execute_python_code` — those pump the game-thread task graph and trip a
`RecursionGuard` assertion that **crashes the editor** when run from inside a tool call.

```python
# PREFERRED — MCP tool (robust, no crash)
# manage_asset(action="import",
#              source_file_path="C:/Images/rocks.jpg",
#              destination_path="/Game/UI/Textures",
#              asset_name="T_Rocks")

# Python equivalent (same safe C++ path)
import unreal
path, err = unreal.AssetDiscoveryService.import_asset(
    "C:/Images/rocks.jpg", "/Game/UI/Textures", "T_Rocks")
print(path or err)

# WRONG — crashes the editor (TaskGraph RecursionGuard assertion)
# tasks = [unreal.AssetImportTask()]; unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
```

Supported image formats: png, jpg, jpeg, bmp, tga, dds, exr, hdr, tiff, tif, psd, pcx.

Need a source image to import? Editor screenshots live under the **project's**
`Saved/Screenshots` (and `Saved/VibeUE/Screenshots`) — not the user's AppData. List them with:

```python
import os, unreal
shots = os.path.join(unreal.Paths.project_saved_dir(), "Screenshots")
print(os.listdir(shots) if os.path.isdir(shots) else "no screenshots yet")
```

### ⚠️ Asset Paths Must Be Content Browser Paths

Use `/Game/...` paths, **not** file system paths.

```python
# WRONG - file system path
asset = unreal.AssetDiscoveryService.find_asset_by_path("C:/Projects/Content/BP_Player.uasset")

# CORRECT - content browser path
asset = unreal.AssetDiscoveryService.find_asset_by_path("/Game/Blueprints/BP_Player")
```

### ⚠️ Never Emulate Move/Rename with Duplicate + Delete

Duplicating creates a new asset identity. References stay pointed at the original asset, so deleting the original after a duplicate can break those references.

Use a real move instead:

```python
import unreal

unreal.AssetDiscoveryService.move_asset(
    "/Game/StateTree/STT_Rotate",
    "/Game/StateTree/Tasks/STT_Rotate"
)

# MCP equivalent
# manage_asset(action="move", source_path="/Game/StateTree/STT_Rotate", destination_path="/Game/StateTree/Tasks/STT_Rotate")
```

### ⚠️ Methods NOT Available

| Does NOT Exist | Alternative |
|----------------|-------------|
| `is_asset_in_use()` | `get_asset_referencers()` → check if empty |
| `rename_asset()` | `move_asset(old_path, new_path)` |
| `export_asset()` | `export_texture()` for textures only |

(`asset_exists(asset_path)` DOES exist — use it for simple existence checks.)

### Creating Widget Blueprints

`unreal.BlueprintService.create_blueprint(name, "UserWidget", path)` creates a proper
`WidgetBlueprint` (it detects UserWidget-derived parents and uses the UMG factory). For
designing the widget afterwards (hierarchy, slots, bindings), load the **umg-widgets** skill.

---

## Workflows

### Search Pattern

```python
import unreal

# By name (partial match) - searches ALL mounted roots (/Game, /Engine, plugins)
results = unreal.AssetDiscoveryService.search_assets("Player", "Blueprint")
for asset in results:
    print(f"{asset.asset_name}: {asset.package_path}")

# "Show me the first N" — manage_asset has no limit param and can return thousands of rows.
# Slice in Python instead of dumping the full list:
for asset in unreal.AssetDiscoveryService.list_assets_in_path("/Engine", "Texture2D")[:10]:
    print(f"{asset.asset_name}: {asset.package_path}")

# By folder
assets = unreal.AssetDiscoveryService.list_assets_in_path("/Game/Blueprints")

# By type only
all_bps = unreal.AssetDiscoveryService.get_assets_by_type("Blueprint")
```

### Search Engine Content

```python
import unreal

# Search engine content (e.g., for built-in meshes like Cube)
engine_assets = unreal.AssetDiscoveryService.list_assets_in_path("/Engine")
cubes = [a for a in engine_assets if "Cube" in str(a.asset_name)]

# Filter by type
meshes = unreal.AssetDiscoveryService.list_assets_in_path("/Engine", "StaticMesh")
```

### Check Exists Pattern

```python
import unreal

asset = unreal.AssetDiscoveryService.find_asset_by_path("/Game/BP_Player")
if asset:
    print(f"Found: {asset.asset_name}")
else:
    print("Not found - create it")
```

### Duplicate Pattern

```python
import unreal

success = unreal.AssetDiscoveryService.duplicate_asset("/Game/BP_Enemy", "/Game/BP_EnemyBoss")
if success:
    print("Duplicated")
```

### Move Pattern

```python
import unreal

success = unreal.AssetDiscoveryService.move_asset(
    "/Game/StateTree/STT_Rotate",
    "/Game/StateTree/Tasks/STT_Rotate"
)
if success:
    print("Moved without breaking references")
```

### Save Pattern

```python
import unreal

# Save specific asset
unreal.AssetDiscoveryService.save_asset("/Game/BP_Player")

# Save all dirty assets
count = unreal.AssetDiscoveryService.save_all_assets()
print(f"Saved {count} assets")
```

### Check References Before Delete

⚠️ `get_asset_referencers` / `get_asset_dependencies` return **lists of plain strings**
(package names like `"/Game/Blueprints/BP_Bird"`), NOT `AssetData` objects — accessing
`.asset_name` on an entry raises `AttributeError: 'str' object has no attribute 'asset_name'`.

```python
import unreal

refs = unreal.AssetDiscoveryService.get_asset_referencers("/Game/SM_Weapon")
if len(refs) == 0:
    unreal.AssetDiscoveryService.delete_asset("/Game/SM_Weapon")
else:
    for ref in refs:          # ref is a str, e.g. "/Game/Blueprints/BP_Player"
        print(f"In use by: {ref}")
```

### "What references X?" Pattern

When the user names an asset loosely (e.g. "IA_Move"), search first — there may be several
matches in different folders — then get referencers for each:

```python
import unreal

for hit in unreal.AssetDiscoveryService.search_assets("IA_Move", "InputAction"):
    pkg = str(hit.package_name)
    refs = unreal.AssetDiscoveryService.get_asset_referencers(pkg)
    print(pkg)
    for r in refs:            # strings, not AssetData
        print("  <-", r)
```

### Import/Export Textures

```python
import unreal

# Import (disk → Content Browser). Returns (asset_path, error); asset_path is "" on failure.
# Pass a destination FOLDER + optional asset name.
path, err = unreal.AssetDiscoveryService.import_asset(
    "C:/Textures/logo.png", "/Game/Textures", "T_Logo")
print(path or err)

# import_texture(src, dest_asset_path) still works (takes a full asset path) and now uses the
# same crash-safe importer under the hood:
unreal.AssetDiscoveryService.import_texture("C:/Textures/logo.png", "/Game/Textures/T_Logo")

# Export (project → file system)
unreal.AssetDiscoveryService.export_texture("/Game/Textures/T_Logo", "C:/Exports/logo.png")
```

> Prefer the `manage_asset` import action in tool flows:
> `manage_asset(action="import", source_file_path="C:/Textures/logo.png", destination_path="/Game/Textures", asset_name="T_Logo")`

### Editor State & Content Browser

```python
import unreal

# Get all open assets
open_assets = unreal.AssetDiscoveryService.get_open_assets()
print(f"Editing {len(open_assets)} assets")

# Check if specific asset is open
if unreal.AssetDiscoveryService.is_asset_open("/Game/BP_Player"):
    print("BP_Player is open")

# Get selected assets in Content Browser
selected = unreal.AssetDiscoveryService.get_content_browser_selections()
for asset in selected:
    print(asset.asset_name)

# Get primary selection — NO out-arg, returns AssetData or None
asset = unreal.AssetDiscoveryService.get_primary_content_browser_selection()
if asset:
    print(f"Selected: {asset.asset_name}")
else:
    print("Nothing selected")

# Open whatever is selected, then confirm it's open
if asset:
    unreal.AssetDiscoveryService.open_asset(str(asset.package_name))
```

### Filter Open Assets by Type

`asset.asset_class_path` is a `TopLevelAssetPath` struct, not a string — compare its `asset_name`:

```python
import unreal

open_assets = unreal.AssetDiscoveryService.get_open_assets()
open_bps = [a for a in open_assets
            if str(a.asset_class_path.asset_name) == "Blueprint"]
for bp in open_bps:
    print(f"{bp.asset_name} at {bp.package_path}")
```

### Create Non-Standard Asset Types (Factory Pattern)

Assets not covered by VibeUE services (e.g., `LandscapeGrassType`) require `AssetToolsHelpers` + a factory:

```python
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# LandscapeGrassType
factory = unreal.LandscapeGrassTypeFactory()
lgt = asset_tools.create_asset("LGT_MyGrass", "/Game/Landscape", unreal.LandscapeGrassType, factory)

# After creation, set properties via set_editor_property, then save
unreal.EditorAssetLibrary.save_asset("/Game/Landscape/LGT_MyGrass")
```

> **Tip:** Use `discover_python_module("unreal", name_filter="Factory")` to find available factories for other asset types.

### Asset Type Filter Accepts Any Loaded Class

The `asset_type` filter (in `search_assets`, `get_assets_by_type`, `list_assets_in_path`, and the
`manage_asset` tool) resolves short class names from **any** loaded module — `InputAction`,
`LandscapeGrassType`, `NiagaraSystem`, etc. — and also accepts full `/Script/Module.Class` paths.
If a typed search unexpectedly returns 0:

1. Class names must match UE class names exactly (e.g., `LandscapeGrassType`, not `GrassType`)
2. Retry without the type filter and check `asset_class` in the results to learn the real class name
3. Use `list_assets_in_path("/Game/SomeFolder")` and filter in Python

### Common Asset Types for Filtering

- `Blueprint` - Blueprint classes
- `WidgetBlueprint` - UMG Widget Blueprints
- `Texture2D` - Texture assets
- `StaticMesh` - Static mesh assets
- `SkeletalMesh` - Skeletal mesh assets
- `Material` - Materials
- `MaterialInstanceConstant` - Material instances
- `DataTable` - Data Tables
- `PrimaryDataAsset` - Primary Data Assets
- `LandscapeGrassType` - Landscape grass/vegetation scatter types
- `LandscapeLayerInfoObject` - Landscape paint layer info

## Sample scripts (run via `execute_python_code`)

- **`scripts/find_and_save.pyx`** — find an asset and duplicate+save it (prefer the manage_asset tool for these ops).
