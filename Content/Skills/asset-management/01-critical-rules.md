# Asset Discovery Critical Rules

**Note:** Method signatures are in `vibeue_apis` from skill loader. This file contains gotchas that discovery can't tell you.

---

## ⚠️ `search_assets` Does NOT Search Engine Content

`search_assets(term, type)` **only searches `/Game/` content**. There is NO third parameter.

```python
# WRONG - will error (only 2 params exist)
assets = unreal.AssetDiscoveryService.search_assets("Cube", "", True)

# CORRECT - use list_assets_in_path for engine content
engine_assets = unreal.AssetDiscoveryService.list_assets_in_path("/Engine")
cubes = [a for a in engine_assets if "Cube" in str(a.asset_name)]
```

---

## ⚠️ UE 5.7 Property Changes (CRITICAL)

AssetData properties changed in UE 5.7. **Discovery tells you the REAL names.**

| WRONG (old) | CORRECT (UE 5.7) |
|-------------|------------------|
| `asset.name` | `asset.asset_name` |
| `asset.path` | `asset.package_path` |
| `asset.asset_class` | `asset.asset_class_path` (deprecated) |

---

## ⚠️ Asset Paths Must Be Content Browser Paths

Use `/Game/...` paths, **not** file system paths.

**WRONG:**
```python
# File system path - WILL NOT WORK
asset = unreal.AssetDiscoveryService.find_asset_by_path(
    "C:/Projects/MyProject/Content/Blueprints/BP_Player.uasset"
)
```

**CORRECT:**
```python
# Content browser path
asset = unreal.AssetDiscoveryService.find_asset_by_path("/Game/Blueprints/BP_Player")
```

---

## ⚠️ Always Check for None/Empty Results

Some methods return None or empty arrays when nothing found.

```python
import unreal

# Check for None
asset = unreal.AssetDiscoveryService.find_asset_by_path("/Game/BP_Missing")
if asset:
    print(f"Found: {asset.asset_name}")
else:
    print("Asset not found")

# Check for empty array
assets = unreal.AssetDiscoveryService.search_assets("NonExistent")
if len(assets) > 0:
    print(f"Found {len(assets)} assets")
else:
    print("No assets found")
```

---

## ⚠️ Check Success Before Using Results

Operations that modify assets return bool for success/failure.

```python
import unreal

success = unreal.AssetDiscoveryService.duplicate_asset(
    "/Game/Blueprints/BP_Enemy",
    "/Game/Blueprints/BP_EnemyStrong"
)

if success:
    # Now safe to use the new asset
    new_asset = unreal.AssetDiscoveryService.find_asset_by_path("/Game/Blueprints/BP_EnemyStrong")
else:
    print("Duplication failed")
```

---

## ⚠️ Methods NOT Available

These methods do **NOT** exist (use alternatives):

| Does NOT Exist | Alternative |
|----------------|-------------|
| `asset_exists()` | `find_asset_by_path()` → check for None |
| `is_asset_in_use()` | `get_asset_referencers()` → check if empty |
| `is_asset_dirty()` | `save_all_assets()` saves all dirty assets |
| `find_assets()` | `search_assets()` or `list_assets_in_path()` |
| `rename_asset()` | Not available - duplicate then delete |
| `move_asset()` | Not available - duplicate to new path then delete |
| `import_asset()` | `import_texture()` for textures only |
| `export_asset()` | `export_texture()` for textures only |

---

## Common Asset Types for Filtering

When using `search_assets()` or `get_assets_by_type()`:

- `Blueprint` - Blueprint classes
- `WidgetBlueprint` - UMG Widget Blueprints
- `Texture2D` - Texture assets
- `StaticMesh` - Static mesh assets
- `SkeletalMesh` - Skeletal mesh assets
- `Material` - Materials
- `MaterialInstanceConstant` - Material instances
- `DataTable` - Data Tables
- `PrimaryDataAsset` - Primary Data Assets
