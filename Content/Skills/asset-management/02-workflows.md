# Asset Management Workflows

Common patterns for searching, managing, and organizing assets.

**NOTE:** In UE 5.7, AssetData properties have changed:
- Use `.asset_name` (not `.name`)
- Use `.package_path` (not `.path`)
- Use `.asset_class_path` (not deprecated `.asset_class`)

---

## Search for Assets

```python
import unreal

# 1. Search by name pattern
print("=== Searching by name ===")
player_assets = unreal.AssetDiscoveryService.search_assets("Player")
for asset in player_assets:
    print(f"{asset.asset_name}: {asset.package_path}")

# 2. Search by type
print("\n=== Searching for blueprints ===")
blueprints = unreal.AssetDiscoveryService.search_assets("", "Blueprint")
for bp in blueprints:
    print(f"{bp.asset_name}: {bp.package_path}")

# 3. List assets in specific folder
print("\n=== Assets in /Game/Meshes ===")
meshes = unreal.AssetDiscoveryService.list_assets_in_path("/Game/Meshes")
for mesh in meshes:
    print(f"{mesh.asset_name}")

# 4. Combined search (name pattern + type filter)
print("\n=== Enemy blueprints ===")
enemy_bps = unreal.AssetDiscoveryService.search_assets("Enemy", "Blueprint")
for bp in enemy_bps:
    print(f"{bp.asset_name}: {bp.package_path}")
```

---

## Check if Asset Exists

```python
import unreal

# Use find_asset_by_path - returns None if not found
asset_path = "/Game/Blueprints/BP_Player"
asset = unreal.AssetDiscoveryService.find_asset_by_path(asset_path)

if asset:
    print(f"Asset exists: {asset.asset_name}")
else:
    print("Asset not found - create it")
```

---

## Find Asset References

```python
import unreal

asset_path = "/Game/Meshes/SM_Weapon"

# 1. Find what this asset depends on
print(f"=== Dependencies of {asset_path} ===")
dependencies = unreal.AssetDiscoveryService.get_asset_dependencies(asset_path)
for dep in dependencies:
    print(f"  Uses: {dep}")

# 2. Find what uses this asset
print(f"\n=== Assets referencing {asset_path} ===")
referencers = unreal.AssetDiscoveryService.get_asset_referencers(asset_path)
if len(referencers) > 0:
    for ref in referencers:
        print(f"  Used by: {ref}")
else:
    print("  No references - safe to delete")
```

---

## Duplicate Assets

```python
import unreal

# 1. Find base asset
base_path = "/Game/Blueprints/BP_Enemy"
base_asset = unreal.AssetDiscoveryService.find_asset_by_path(base_path)

if base_asset:
    # 2. Create variants
    variants = [
        "/Game/Blueprints/BP_EnemyWeak",
        "/Game/Blueprints/BP_EnemyStrong",
        "/Game/Blueprints/BP_EnemyBoss"
    ]

    for variant_path in variants:
        success = unreal.AssetDiscoveryService.duplicate_asset(base_path, variant_path)
        if success:
            print(f"Created variant: {variant_path}")
        else:
            print(f"Failed to create: {variant_path}")
```

---

## Clean Up Unused Assets

```python
import unreal

# 1. Get all assets in a folder
folder_path = "/Game/Archive"
assets = unreal.AssetDiscoveryService.list_assets_in_path(folder_path)

print(f"Checking {len(assets)} assets for usage...")

# 2. Find unused assets
unused_assets = []
for asset in assets:
    asset_full_path = f"{asset.package_path}/{asset.asset_name}"
    refs = unreal.AssetDiscoveryService.get_asset_referencers(asset_full_path)
    if len(refs) == 0:
        unused_assets.append(asset)

print(f"Found {len(unused_assets)} unused assets")

# 3. Delete unused assets (with confirmation)
if len(unused_assets) > 0:
    print("\nUnused assets:")
    for asset in unused_assets:
        print(f"  - {asset.asset_name}")

    # Uncomment to actually delete:
    # for asset in unused_assets:
    #     asset_full_path = f"{asset.package_path}/{asset.asset_name}"
    #     success = unreal.AssetDiscoveryService.delete_asset(asset_full_path)
    #     if success:
    #         print(f"Deleted: {asset.asset_name}")

    print("\n(Uncomment delete code to actually remove assets)")
```

---

## Import Textures

```python
import unreal

# Import a single texture
success = unreal.AssetDiscoveryService.import_texture(
    "C:/Textures/character.png",
    "/Game/Textures/T_Character"
)
if success:
    print("Texture imported successfully")
```

---

## Export Textures

```python
import unreal

# Export a texture
success = unreal.AssetDiscoveryService.export_texture(
    "/Game/Textures/T_Character",
    "C:/Exports/character.png"
)
if success:
    print("Texture exported successfully")
```

---

## Generate Asset Report

```python
import unreal

# 1. Get all game assets
all_assets = unreal.AssetDiscoveryService.list_assets_in_path("/Game")

# 2. Count by class (using asset_class_path)
class_counts = {}

for asset in all_assets:
    # Get class name from asset_class_path
    class_name = str(asset.asset_class_path).split(".")[-1] if asset.asset_class_path else "Unknown"
    
    if class_name not in class_counts:
        class_counts[class_name] = 0
    class_counts[class_name] += 1

# 3. Generate report
print("=== Asset Report ===\n")
print(f"Total assets: {len(all_assets)}\n")

print("Assets by type:")
for asset_class, count in sorted(class_counts.items(), key=lambda x: x[1], reverse=True):
    print(f"  {asset_class}: {count}")
```

---

## Validate Asset Paths

```python
import unreal

# 1. List of expected assets
expected_assets = [
    "/Game/Blueprints/BP_Player",
    "/Game/Blueprints/BP_Enemy",
    "/Game/Meshes/SM_Weapon",
    "/Game/Materials/M_Character"
]

# 2. Check if each asset exists
print("=== Asset Validation ===\n")
missing_assets = []

for asset_path in expected_assets:
    asset = unreal.AssetDiscoveryService.find_asset_by_path(asset_path)
    if asset:
        print(f"✓ {asset_path}")
    else:
        print(f"✗ {asset_path} - MISSING!")
        missing_assets.append(asset_path)

# 3. Report
if len(missing_assets) > 0:
    print(f"\n{len(missing_assets)} assets are missing:")
    for asset_path in missing_assets:
        print(f"  - {asset_path}")
else:
    print("\nAll assets present!")
```

---

## Save Assets

```python
import unreal

# Save all dirty assets at once
count = unreal.AssetDiscoveryService.save_all_assets()
print(f"Saved {count} assets")

# Or save specific asset:
success = unreal.AssetDiscoveryService.save_asset("/Game/Blueprints/BP_Player")
if success:
    print("Asset saved")
```

---

## Common Patterns Summary

### Pattern 1: Search → Filter → Process
```python
import unreal

# Search
assets = unreal.AssetDiscoveryService.search_assets("", "Blueprint")

# Filter
enemy_bps = [a for a in assets if "Enemy" in a.asset_name]

# Process
for bp in enemy_bps:
    # Do something
    pass
```

### Pattern 2: Check Exists → Create/Use
```python
import unreal

asset_path = "/Game/Blueprints/BP_Player"
asset = unreal.AssetDiscoveryService.find_asset_by_path(asset_path)
if asset:
    # Asset exists - use it
    print(f"Found: {asset.asset_name}")
else:
    # Asset doesn't exist - create new
    pass
```

### Pattern 3: Check References Before Delete
```python
import unreal

asset_path = "/Game/Meshes/SM_Old"
refs = unreal.AssetDiscoveryService.get_asset_referencers(asset_path)
if len(refs) == 0:
    # Safe to delete
    unreal.AssetDiscoveryService.delete_asset(asset_path)
else:
    print("Asset is in use - cannot delete")
```

### Pattern 4: Import → Verify → Use
```python
import unreal

# Import texture
success = unreal.AssetDiscoveryService.import_texture(file_path, asset_path)

# Verify
if success:
    asset = unreal.AssetDiscoveryService.find_asset_by_path(asset_path)
    if asset:
        print(f"Imported: {asset.asset_name}")
else:
    print("Import failed")
```
