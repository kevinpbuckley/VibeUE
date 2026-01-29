# Asset Management Workflows

---

## Search Pattern

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

---

## Search Engine Content

⚠️ **`search_assets` does NOT search engine content!** Use `list_assets_in_path("/Engine")`:

```python
import unreal

# Search engine content (e.g., for built-in meshes like Cube)
engine_assets = unreal.AssetDiscoveryService.list_assets_in_path("/Engine")

# Filter by name
cubes = [a for a in engine_assets if "Cube" in str(a.asset_name)]
print(f"Found {len(cubes)} cube assets")

# Filter by type
meshes = unreal.AssetDiscoveryService.list_assets_in_path("/Engine", "StaticMesh")
```

---

## Check Exists Pattern

```python
import unreal

asset = unreal.AssetDiscoveryService.find_asset_by_path("/Game/BP_Player")
if asset:
    print(f"Found: {asset.asset_name}")
else:
    print("Not found - create it")
```

---

## Check References Before Delete

```python
import unreal

refs = unreal.AssetDiscoveryService.get_asset_referencers("/Game/SM_Weapon")
if len(refs) == 0:
    unreal.AssetDiscoveryService.delete_asset("/Game/SM_Weapon")
    print("Deleted")
else:
    print(f"In use by {len(refs)} assets - cannot delete")
```

---

## Duplicate Pattern

```python
import unreal

success = unreal.AssetDiscoveryService.duplicate_asset(
    "/Game/BP_Enemy",
    "/Game/BP_EnemyBoss"
)
if success:
    print("Duplicated")
```

---

## Save Pattern

```python
import unreal

# Save specific asset
unreal.AssetDiscoveryService.save_asset("/Game/BP_Player")

# Save all dirty assets
count = unreal.AssetDiscoveryService.save_all_assets()
print(f"Saved {count} assets")
```

---

## Import/Export Textures

```python
import unreal

# Import (file system → project)
unreal.AssetDiscoveryService.import_texture(
    "C:/Textures/logo.png",
    "/Game/Textures/T_Logo"
)

# Export (project → file system)
unreal.AssetDiscoveryService.export_texture(
    "/Game/Textures/T_Logo",
    "C:/Exports/logo.png"
)
```

---

## Open Asset in Editor

```python
import unreal

unreal.AssetDiscoveryService.open_asset("/Game/BP_Player")
```

---

## Get Open Assets & Content Browser Selections

```python
import unreal

# Get the currently active/focused asset
active = unreal.AssetData()
if unreal.AssetDiscoveryService.get_active_asset(active):
    print(f"Currently focused: {active.asset_name}")

# Get all currently open assets in editors
open_assets = unreal.AssetDiscoveryService.get_open_assets()
print(f"Currently editing {len(open_assets)} assets:")
for asset in open_assets:
    print(f"  - {asset.asset_name} ({asset.package_path})")

# Check if specific asset is open
if unreal.AssetDiscoveryService.is_asset_open("/Game/BP_Player"):
    print("BP_Player is currently being edited")
else:
    print("BP_Player is not open")

# Get all selected assets in Content Browser
selected = unreal.AssetDiscoveryService.get_content_browser_selections()
if len(selected) > 0:
    print(f"Selected {len(selected)} assets in Content Browser:")
    for asset in selected:
        print(f"  - {asset.asset_name}")
else:
    print("No assets selected in Content Browser")

# Get the primary selection (first selected item)
primary = unreal.AssetData()
if unreal.AssetDiscoveryService.get_primary_content_browser_selection(primary):
    print(f"Primary selection: {primary.asset_name} at {primary.package_path}")
    
    # You can now work with the selected asset
    if "Blueprint" in str(primary.asset_class_path):
        print("Selected a Blueprint - opening it...")
        unreal.AssetDiscoveryService.open_asset(primary.package_path.to_string())
else:
    print("Nothing selected in Content Browser")
```
