---
name: asset-management
display_name: Asset Discovery & Management
description: Search, find, open, duplicate, save, delete, import, and export assets
vibeue_classes:
  - AssetDiscoveryService
unreal_classes:
  - EditorAssetLibrary
---

# Asset Discovery & Management Skill

## Critical Rules

### ⚠️ `search_assets` Does NOT Search Engine Content

`search_assets(term, type)` **only searches `/Game/` content**. There is NO third parameter.

```python
# WRONG - will error (only 2 params exist)
assets = unreal.AssetDiscoveryService.search_assets("Cube", "", True)

# CORRECT - use list_assets_in_path for engine content
engine_assets = unreal.AssetDiscoveryService.list_assets_in_path("/Engine")
cubes = [a for a in engine_assets if "Cube" in str(a.asset_name)]
```

### ⚠️ UE 5.7 Property Changes

| WRONG (old) | CORRECT (UE 5.7) |
|-------------|------------------|
| `asset.name` | `asset.asset_name` |
| `asset.path` | `asset.package_path` |
| `asset.asset_class` | `asset.asset_class_path` |

### ⚠️ Asset Paths Must Be Content Browser Paths

Use `/Game/...` paths, **not** file system paths.

```python
# WRONG - file system path
asset = unreal.AssetDiscoveryService.find_asset_by_path("C:/Projects/Content/BP_Player.uasset")

# CORRECT - content browser path
asset = unreal.AssetDiscoveryService.find_asset_by_path("/Game/Blueprints/BP_Player")
```

### ⚠️ Methods NOT Available

| Does NOT Exist | Alternative |
|----------------|-------------|
| `asset_exists()` | `find_asset_by_path()` → check for None |
| `is_asset_in_use()` | `get_asset_referencers()` → check if empty |
| `rename_asset()` | Duplicate then delete |
| `move_asset()` | Duplicate to new path then delete |
| `import_asset()` | `import_texture()` for textures only |
| `export_asset()` | `export_texture()` for textures only |

---

## Workflows

### Search Pattern

```python
import unreal

# By name (partial match) - ONLY searches /Game/ content
results = unreal.AssetDiscoveryService.search_assets("Player", "Blueprint")
for asset in results:
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

```python
import unreal

refs = unreal.AssetDiscoveryService.get_asset_referencers("/Game/SM_Weapon")
if len(refs) == 0:
    unreal.AssetDiscoveryService.delete_asset("/Game/SM_Weapon")
else:
    print(f"In use by {len(refs)} assets")
```

### Import/Export Textures

```python
import unreal

# Import (file system → project)
unreal.AssetDiscoveryService.import_texture("C:/Textures/logo.png", "/Game/Textures/T_Logo")

# Export (project → file system)
unreal.AssetDiscoveryService.export_texture("/Game/Textures/T_Logo", "C:/Exports/logo.png")
```

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

# Get primary selection
asset = unreal.AssetData()
if unreal.AssetDiscoveryService.get_primary_content_browser_selection(asset):
    print(f"Selected: {asset.asset_name}")
```

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
