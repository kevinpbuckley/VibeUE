# AssetDiscoveryService API Reference

All methods are called via `unreal.AssetDiscoveryService.<method_name>(...)`.

**ALWAYS use `discover_python_class("unreal.AssetDiscoveryService")` for parameter details before calling.**

---

## ⚠️ UE 5.7 Property Changes

AssetData properties have changed in UE 5.7:
- Use `.asset_name` (not `.name`)
- Use `.package_path` (not `.path`)
- `.asset_class` is **DEPRECATED** - Use `.asset_class_path` instead

---

## Search and Discovery

### search_assets(search_term, asset_type="")
Search for assets by name pattern and optional type filter.

**Returns:** Array[AssetData]

**Example:**
```python
import unreal

# Search by name pattern
results = unreal.AssetDiscoveryService.search_assets("Player", "Blueprint")
for asset in results:
    print(f"{asset.asset_name}: {asset.package_path}")

# Search for any asset containing "Enemy"
enemies = unreal.AssetDiscoveryService.search_assets("Enemy")

# Search for specific type only (empty search term)
all_textures = unreal.AssetDiscoveryService.search_assets("", "Texture2D")
```

### list_assets_in_path(path, asset_type="")
List all assets in a specific path (recursive).

**Returns:** Array[AssetData]

**Example:**
```python
import unreal

# List all assets in a folder
assets = unreal.AssetDiscoveryService.list_assets_in_path("/Game/Blueprints")
for asset in assets:
    print(f"{asset.asset_name}")

# List only blueprints in a folder
blueprints = unreal.AssetDiscoveryService.list_assets_in_path("/Game/Blueprints", "Blueprint")
```

### get_assets_by_type(asset_type)
Get all assets of a specific type.

**Returns:** Array[AssetData]

**Example:**
```python
import unreal

# Get all blueprints in project
blueprints = unreal.AssetDiscoveryService.get_assets_by_type("Blueprint")
print(f"Found {len(blueprints)} blueprints")

# Get all static meshes
meshes = unreal.AssetDiscoveryService.get_assets_by_type("StaticMesh")
```

### find_asset_by_path(asset_path)
Find a specific asset by its exact path.

**Returns:** AssetData or None

**Example:**
```python
import unreal

# Check if asset exists (returns None if not found)
asset = unreal.AssetDiscoveryService.find_asset_by_path("/Game/Blueprints/BP_Player")
if asset:
    print(f"Found: {asset.asset_name}")
else:
    print("Asset not found")
```

---

## Asset Information

### get_asset_dependencies(asset_path)
Get assets that this asset depends on (hard references).

**Returns:** Array[str] (asset paths)

**Example:**
```python
import unreal

deps = unreal.AssetDiscoveryService.get_asset_dependencies("/Game/Blueprints/BP_Player")
print(f"Dependencies:")
for dep in deps:
    print(f"  - {dep}")
```

### get_asset_referencers(asset_path)
Get assets that reference this asset (hard references).

**Returns:** Array[str] (asset paths)

**Example:**
```python
import unreal

refs = unreal.AssetDiscoveryService.get_asset_referencers("/Game/Meshes/SM_Cube")
print(f"Referenced by:")
for ref in refs:
    print(f"  - {ref}")

# Check if asset is in use (has any referencers)
if len(refs) > 0:
    print("Asset is in use")
else:
    print("Asset is not referenced - safe to delete")
```

---

## Asset Operations

### duplicate_asset(source_path, destination_path)
Duplicate an asset to a new location.

**Returns:** bool

**Example:**
```python
import unreal

success = unreal.AssetDiscoveryService.duplicate_asset(
    "/Game/Blueprints/BP_Enemy",
    "/Game/Blueprints/BP_EnemyStrong"
)
if success:
    print("Asset duplicated successfully")
```

### delete_asset(asset_path)
Delete an asset.

**Returns:** bool

**Example:**
```python
import unreal

success = unreal.AssetDiscoveryService.delete_asset("/Game/Temp/BP_TestActor")
if success:
    print("Asset deleted")
else:
    print("Failed to delete asset")
```

### open_asset(asset_path)
Open an asset in its appropriate editor.

**Returns:** bool

**Example:**
```python
import unreal

success = unreal.AssetDiscoveryService.open_asset("/Game/Blueprints/BP_Player")
if success:
    print("Asset opened in editor")
```

---

## Save Operations

### save_asset(asset_path)
Save a specific asset.

**Returns:** bool

**Example:**
```python
import unreal

success = unreal.AssetDiscoveryService.save_asset("/Game/Blueprints/BP_Player")
if success:
    print("Asset saved")
```

### save_all_assets()
Save all dirty (modified) assets.

**Returns:** int (number of assets saved)

**Example:**
```python
import unreal

count = unreal.AssetDiscoveryService.save_all_assets()
print(f"Saved {count} assets")
```

---

## Import and Export (Textures Only)

### import_texture(source_file_path, destination_path)
Import a texture from the file system into the project.

**Returns:** bool

**Example:**
```python
import unreal

success = unreal.AssetDiscoveryService.import_texture(
    "C:/Images/logo.png",
    "/Game/Textures/Logo"
)
if success:
    print("Texture imported")
```

### export_texture(asset_path, export_file_path)
Export a texture to the file system.

**Returns:** bool

**Example:**
```python
import unreal

success = unreal.AssetDiscoveryService.export_texture(
    "/Game/Textures/MyTexture",
    "C:/Exports/texture.png"
)
if success:
    print("Texture exported")
```

---

## Critical Rules

### Return Types Are Structs (AssetData)

Methods return `AssetData` struct objects. Access properties with dot notation.

**WRONG:**
```python
assets = unreal.AssetDiscoveryService.search_assets("Enemy")
for asset in assets:
    print(asset["name"])  # ERROR! asset is struct, not dict
```

**CORRECT:**
```python
assets = unreal.AssetDiscoveryService.search_assets("Enemy")
for asset in assets:
    print(asset.asset_name)  # Use .asset_name property
    print(asset.package_path)
    # Note: .asset_class is deprecated in UE 5.7, use .asset_class_path
```

### Always Check for None/Empty

Some methods return None or empty results when nothing is found.

**CORRECT:**
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

### Asset Paths Must Be Content Browser Paths

Use `/Game/...` paths, not file system paths.

**WRONG:**
```python
# File system path - WILL NOT WORK
asset = unreal.AssetDiscoveryService.find_asset_by_path("C:/Projects/MyProject/Content/Blueprints/BP_Player.uasset")
```

**CORRECT:**
```python
# Content browser path
asset = unreal.AssetDiscoveryService.find_asset_by_path("/Game/Blueprints/BP_Player")
```

### Check Success Before Using Results

Operations that modify assets return bool for success/failure.

**CORRECT:**
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

## Methods NOT Available

The following methods do **NOT** exist in the Python API (contrary to some documentation):

| Method | Alternative |
|--------|-------------|
| `asset_exists()` | Use `find_asset_by_path()` and check for None |
| `is_asset_in_use()` | Use `get_asset_referencers()` and check if empty |
| `is_asset_dirty()` | Not available - use `save_all_assets()` |
| `find_assets()` | Use `search_assets()` or `list_assets_in_path()` |
| `directory_exists()` | Not available in Python API |
| `create_directory()` | Not available in Python API |
| `rename_asset()` | Not available in Python API |
| `move_asset()` | Not available in Python API |
| `import_asset()` | Use `import_texture()` for textures only |
| `export_asset()` | Use `export_texture()` for textures only |
