---
name: asset-management
display_name: Asset Discovery & Management
description: Import/export textures crash-safely, query the Content Browser selection, and check if an asset is open (AssetDiscoveryService). Search, load, save, move, rename, duplicate, and delete assets are handled by Unreal's native AssetTools toolset or EditorAssetLibrary. Use when the user asks to import an image from disk, export a texture, query the Content Browser selection, or check whether an asset is open in an editor.
vibeue_classes:
  - AssetDiscoveryService
unreal_classes:
  - EditorAssetLibrary
  - AssetRegistryHelpers
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "asset-management"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Asset Discovery & Management Skill

> 🔀 **Engine owns general asset ops now.** In the Unreal 5.8 consolidation, searching, loading,
> saving, moving, renaming, duplicating, and deleting assets moved to Unreal's native **`AssetTools`**
> toolset (reach it via `call_tool`; run `describe_toolset` for its actions) — or you can drive
> `unreal.EditorAssetLibrary` / `unreal.AssetRegistryHelpers` directly from `execute_python_code`.
> The old **`manage_asset` MCP tool is GONE.** VibeUE's `AssetDiscoveryService` was trimmed to only
> the crash-safe delta the engine doesn't provide:
>
> | Kept on `AssetDiscoveryService` | Purpose |
> |---|---|
> | `import_asset(src, dest_folder, name)` → `(path, err)` | Crash-safe image import (folder + name) |
> | `import_texture(src, dest_asset_path)` | Crash-safe image import (full asset path) |
> | `export_texture(asset_path, file_path)` | Export a texture to disk |
> | `get_primary_content_browser_selection()` → `AssetData or None` | Primary Content Browser selection |
> | `is_asset_open(asset_path)` → `bool` | Whether an asset is open in an editor |
>
> Everything else below (`search_assets`, `list_assets_in_path`, `move_asset`, `duplicate_asset`,
> `delete_asset`, `save_asset`, `get_open_assets`, `get_asset_referencers`, `find_asset_by_path`,
> `asset_exists`, …) was **removed** from `AssetDiscoveryService` — use the engine `AssetTools`
> toolset or `EditorAssetLibrary` / the Asset Registry.

## Data Assets & Data Tables — no dedicated VibeUE service (issues #451, #452)

There is **no `DataAssetService` / `DataTableService`**. Drive them natively:

```python
import unreal

# --- Data Assets (UPrimaryDataAsset / UDataAsset subclasses) ---
# Create: engine DataAssetTools toolset via call_tool (describe_toolset for the action), OR a factory.
da = unreal.load_asset("/Game/Data/DA_Thing")
val = da.get_editor_property("MyField")          # read
da.set_editor_property("MyField", 42)            # write — works for any UPROPERTY
unreal.EditorAssetLibrary.save_loaded_asset(da)

# --- Data Tables ---
dt = unreal.load_asset("/Game/Data/DT_Items")
names = unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)   # row keys
# Read/write rows with the editor DataTable API:
#   unreal.DataTableFunctionLibrary.evaluate_curve_table_row(...) / get_data_table_column_as_string(dt, "Col")
# Engine DataTableTools (call_tool) covers add/remove/get rows + import/export CSV/JSON.
```

> ⚠️ **Data Table gotchas (verified, #452):** the row **key** ("Name") is reported as a column by
> some schema readers — it is the row id, not a data field. Writing a row with the wrong value type
> can be silently coerced — read the row back to confirm. There is no `clear_rows`; remove rows
> individually (engine `DataTableTools`) or re-import an empty CSV. An empty `GameplayTag` field
> serializes as `"None"`.

## Critical Rules

### ⚠️ Out-Params Become Return Values in Python — Never Pass an `AssetData` Argument

`get_primary_content_browser_selection` is shaped like `bool Func(FAssetData& Out)` in C++ and is
exposed to Python as `() -> AssetData or None`. Passing an `AssetData` argument raises `TypeError`.

```python
# WRONG - TypeError: takes no arguments (1 given)
asset = unreal.AssetData()
unreal.AssetDiscoveryService.get_primary_content_browser_selection(asset)

# CORRECT - call with no out-arg, check the return for None
asset = unreal.AssetDiscoveryService.get_primary_content_browser_selection()
if asset:
    print(asset.asset_name)
```

### 🔀 Where each operation lives now

| Operation | Use |
|-----------|-----|
| Search / find / list assets | engine **`AssetTools`** toolset via `call_tool`, or `unreal.AssetRegistryHelpers.get_asset_registry()` |
| Load / save / save-all | `unreal.EditorAssetLibrary.load_asset` / `save_asset` / `save_directory`, or `AssetTools` |
| Move / rename / duplicate / delete | `unreal.EditorAssetLibrary.rename_asset` (move), `duplicate_asset`, `delete_asset`, or `AssetTools` |
| Existence check | `unreal.EditorAssetLibrary.does_asset_exist(path)` |
| Referencers / dependencies | `unreal.AssetRegistryHelpers.get_asset_registry().get_referencers(...)` |
| Open an asset / list ALL open editors | Epic `EditorAppToolset` via `call_tool` (see below) |
| Import image from disk (crash-safe) | `unreal.AssetDiscoveryService.import_asset` / `import_texture` (**stay on VibeUE — see below**) |
| Export a texture to disk | `unreal.AssetDiscoveryService.export_texture` |
| Primary Content Browser selection | `unreal.AssetDiscoveryService.get_primary_content_browser_selection()` |
| Is an asset open in an editor | `unreal.AssetDiscoveryService.is_asset_open(path)` |

### 🔀 ALL open assets / ALL selections / open an asset — use Epic's `EditorAppToolset`

VibeUE covers the **single/primary** queries above; the **list-all** and **open** operations live on
Epic's native `EditorToolset.EditorAppToolset` (call via `call_tool`; returns `{"returnValue": [...]}`
of package-path strings). These are NOT Python-bound — they only work through `call_tool`:

| Need | Call |
|---|---|
| All assets open in editors | `call_tool("GetOpenAssets", "EditorToolset.EditorAppToolset")` |
| All Content Browser selections | `call_tool("GetSelectedAssets", "EditorToolset.EditorAppToolset")` |
| Open an asset in its editor | `call_tool("OpenEditorForAsset", "EditorToolset.EditorAppToolset", {"assetPath": "/Game/.../BP_X"})` |

```python
# "Show me all open Blueprints" = Epic GetOpenAssets + a type filter
opens = call_tool("GetOpenAssets", "EditorToolset.EditorAppToolset")["returnValue"]
bps = [p for p in opens if isinstance(unreal.load_asset(p), unreal.Blueprint)]

# "Open the selected asset" = VibeUE primary-selection + Epic OpenEditorForAsset
sel = unreal.AssetDiscoveryService.get_primary_content_browser_selection()
if sel:
    call_tool("OpenEditorForAsset", "EditorToolset.EditorAppToolset",
              {"assetPath": str(sel.package_name)})
```

### 🚨 Never list broad paths — a `/Game` listing can return 30,000+ assets

Listing every asset under `/Game` returns tens of thousands of entries and floods the conversation.
Whether you go through the engine `AssetTools` toolset or `EditorAssetLibrary.list_assets`, always
narrow first:

- Filter by a **specific subfolder** (e.g. `/Game/Blueprints/HUD`), and by class where supported.
- If you only need a count or sample, slice in Python before printing — never print the full array.

### ⚠️ `EditorAssetLibrary` Save Methods

There is **no `save_dirty_assets`** (`AttributeError`). The real options:
`unreal.EditorAssetLibrary.save_asset(path)`, `save_directory(dir)`, `save_loaded_asset(obj)`,
`save_loaded_assets([objs])`. To save everything dirty, iterate or use the engine `AssetTools`
toolset's save action.

### ⚠️ `EditorAssetLibrary.list_assets` Has No Class Filter

`unreal.EditorAssetLibrary.list_assets(directory_path, recursive=True, include_folder=False)`
returns **path strings only** — there is no `asset_class_names` or any other class-filter kwarg
(`TypeError: invalid keyword argument`). To list assets of one type, either filter the returned
paths in Python (load each and check its class), or query the Asset Registry with a class filter
via `unreal.AssetRegistryHelpers.get_asset_registry().get_assets(...)`.

### ⚠️ UE 5.8 `AssetData` Property Names

`get_primary_content_browser_selection()` returns an `AssetData`. Read it with:

| WRONG (old) | CORRECT |
|-------------|---------|
| `asset.name` | `asset.asset_name` |
| `asset.path` | `asset.package_path` |
| `asset.asset_class` | `asset.asset_class_path` (a `TopLevelAssetPath`; compare `.asset_name`) |
| `asset.object_path` | `f"{asset.package_name}.{asset.asset_name}"` |

`AssetData` has **no** `object_path` attribute (`AttributeError`). Build it from `package_name` +
`asset_name`, or just use `str(asset.package_name)`.

### ⚠️ Importing Image Files From Disk — Stay on `AssetDiscoveryService`

To bring an image file from disk into the Content Browser, use **`AssetDiscoveryService.import_asset`**
(or `import_texture`). Do **NOT** call `unreal.AssetToolsHelpers...import_asset_tasks` or
`ImportAssets` from `execute_python_code` — those pump the game-thread task graph and trip a
`RecursionGuard` assertion that **crashes the editor** when run from inside a tool call.
`AssetDiscoveryService` uses the texture factory's direct binary path, which is safe.

```python
import unreal

# Crash-safe import (disk → Content Browser). Returns (asset_path, error); path is "" on failure.
path, err = unreal.AssetDiscoveryService.import_asset(
    "C:/Images/rocks.jpg", "/Game/UI/Textures", "T_Rocks")
print(path or err)

# WRONG — crashes the editor (TaskGraph RecursionGuard assertion)
# tasks = [unreal.AssetImportTask()]; unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
```

Supported image formats: png, jpg, jpeg, bmp, tga, dds, exr, hdr, tiff, tif, psd, pcx.

Need a source image to import? Editor screenshots live under the **project's**
`Saved/Screenshots` (and `Saved/VibeUE/Screenshots`):

```python
import os, unreal
shots = os.path.join(unreal.Paths.project_saved_dir(), "Screenshots")
print(os.listdir(shots) if os.path.isdir(shots) else "no screenshots yet")
```

### ⚠️ Asset Paths Must Be Content Browser Paths

Use `/Game/...` paths, **not** file system paths, everywhere except the disk side of import/export.

```python
# WRONG - file system path
unreal.EditorAssetLibrary.does_asset_exist("C:/Projects/Content/BP_Player.uasset")

# CORRECT - content browser path
unreal.EditorAssetLibrary.does_asset_exist("/Game/Blueprints/BP_Player")
```

### ⚠️ Never Emulate Move/Rename with Duplicate + Delete

Duplicating creates a new asset identity. References stay pointed at the original, so deleting the
original after a duplicate can break those references. Use a real move/rename (which fixes up
references) instead:

```python
import unreal

# EditorAssetLibrary.rename_asset performs a move + reference fixup
unreal.EditorAssetLibrary.rename_asset(
    "/Game/StateTree/STT_Rotate",
    "/Game/StateTree/Tasks/STT_Rotate")
```

(The engine `AssetTools` toolset exposes an equivalent move/rename action via `call_tool`.)

### Creating Widget Blueprints

`unreal.BlueprintService.create_blueprint(name, "UserWidget", path)` creates a proper
`WidgetBlueprint` (it detects UserWidget-derived parents and uses the UMG factory). For designing
the widget afterwards (hierarchy, slots, bindings), load the **umg-widgets** skill.

---

## Workflows

### Search Pattern

Searching is an engine **`AssetTools`** job (via `call_tool`) — or query the Asset Registry directly:

```python
import unreal

ar = unreal.AssetRegistryHelpers.get_asset_registry()

# All Blueprints under /Game (recursive). Use a class filter and a narrow path to avoid huge results.
f = unreal.ARFilter(
    package_paths=["/Game/Blueprints"],
    class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "Blueprint")],
    recursive_paths=True)
for a in ar.get_assets(f)[:25]:
    print(f"{a.asset_name}: {a.package_path}")
```

### Check Exists Pattern

```python
import unreal

if unreal.EditorAssetLibrary.does_asset_exist("/Game/BP_Player"):
    print("Found")
else:
    print("Not found - create it")
```

### Duplicate Pattern

```python
import unreal

dup = unreal.EditorAssetLibrary.duplicate_asset("/Game/BP_Enemy", "/Game/BP_EnemyBoss")
if dup:
    print("Duplicated")
```

### Move / Rename Pattern

```python
import unreal

ok = unreal.EditorAssetLibrary.rename_asset(
    "/Game/StateTree/STT_Rotate",
    "/Game/StateTree/Tasks/STT_Rotate")
print("Moved (references fixed up)" if ok else "Move failed")
```

### Save Pattern

```python
import unreal

# Save a specific asset
unreal.EditorAssetLibrary.save_asset("/Game/BP_Player")

# Save a whole directory of dirty assets
unreal.EditorAssetLibrary.save_directory("/Game/Blueprints")
```

### Check References Before Delete

Asset Registry referencer queries return **package-name strings**, not `AssetData`.

```python
import unreal

ar = unreal.AssetRegistryHelpers.get_asset_registry()
refs = ar.get_referencers(
    "/Game/SM_Weapon",
    unreal.AssetRegistryDependencyOptions(include_hard_package_references=True))
if not refs:
    unreal.EditorAssetLibrary.delete_asset("/Game/SM_Weapon")
else:
    for ref in refs:          # ref is a package name, e.g. "/Game/Blueprints/BP_Player"
        print(f"In use by: {ref}")
```

### Import / Export Textures (VibeUE — crash-safe)

```python
import unreal

# Import (disk → Content Browser). Returns (asset_path, error); asset_path is "" on failure.
# Pass a destination FOLDER + optional asset name.
path, err = unreal.AssetDiscoveryService.import_asset(
    "C:/Textures/logo.png", "/Game/Textures", "T_Logo")
print(path or err)

# import_texture(src, dest_asset_path) takes a full asset path and uses the same crash-safe importer:
unreal.AssetDiscoveryService.import_texture("C:/Textures/logo.png", "/Game/Textures/T_Logo")

# Export (project → file system)
unreal.AssetDiscoveryService.export_texture("/Game/Textures/T_Logo", "C:/Exports/logo.png")
```

### Editor State & Content Browser (VibeUE)

```python
import unreal

# Is a specific asset open in an editor?
if unreal.AssetDiscoveryService.is_asset_open("/Game/BP_Player"):
    print("BP_Player is open")

# Primary Content Browser selection — NO out-arg, returns AssetData or None
asset = unreal.AssetDiscoveryService.get_primary_content_browser_selection()
if asset:
    print(f"Selected: {asset.asset_name} at {asset.package_path}")
    # Open it via the AssetEditorSubsystem (engine), loading the object first:
    obj = unreal.EditorAssetLibrary.load_asset(str(asset.package_name))
    unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([obj])
else:
    print("Nothing selected")
```

> For the full multi-selection set, open-editor lists, and search, use the engine `AssetTools` /
> `AssetEditorSubsystem` toolsets via `call_tool`.

### Inspect the Selection's Class

`asset.asset_class_path` is a `TopLevelAssetPath` struct, not a string — compare its `asset_name`:

```python
import unreal

asset = unreal.AssetDiscoveryService.get_primary_content_browser_selection()
if asset and str(asset.asset_class_path.asset_name) == "Blueprint":
    print(f"{asset.asset_name} is a Blueprint")
```

### Create Non-Standard Asset Types (Factory Pattern)

Assets not covered by a VibeUE service (e.g., `LandscapeGrassType`) require `AssetToolsHelpers` + a
factory. (The `create_asset` factory path is safe — only the import task-graph APIs crash.)

```python
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

factory = unreal.LandscapeGrassTypeFactory()
lgt = asset_tools.create_asset("LGT_MyGrass", "/Game/Landscape", unreal.LandscapeGrassType, factory)

# Set properties via set_editor_property, then save
unreal.EditorAssetLibrary.save_asset("/Game/Landscape/LGT_MyGrass")
```

> **Tip:** Use `discover_python_module("unreal", name_filter="Factory")` to find available factories.

### Common Asset Class Names for Filtering

- `Blueprint`, `WidgetBlueprint`
- `Texture2D`, `Material`, `MaterialInstanceConstant`
- `StaticMesh`, `SkeletalMesh`
- `DataTable`, `PrimaryDataAsset`
- `LandscapeGrassType`, `LandscapeLayerInfoObject`

Class names must match UE class names exactly (e.g., `LandscapeGrassType`, not `GrassType`). When a
typed Asset Registry query unexpectedly returns 0, retry without the class filter and inspect each
result's `asset_class_path.asset_name` to learn the real class name.

## Sample scripts (run via `execute_python_code`)

- **`scripts/find_and_save.txt`** — find an asset (Asset Registry / `EditorAssetLibrary`) and duplicate + save it.
