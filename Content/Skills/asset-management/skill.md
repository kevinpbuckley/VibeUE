---
name: asset-management
display_name: Asset Discovery & Management
description: Search, find, open, duplicate, save, delete, import, and export assets
services:
  - AssetDiscoveryService
keywords:
  - asset
  - search
  - find
  - duplicate
  - save
  - delete
  - import
  - export
  - texture
  - content browser
auto_load_keywords: []
---

# Asset Discovery & Management

This skill provides comprehensive documentation for discovering and managing assets in Unreal Engine using the AssetDiscoveryService.

## What's Included

- **AssetDiscoveryService API**: Search, find, open, save, delete, import, export assets
- **Workflows**: Common patterns for asset operations

## When to Use This Skill

Load this skill when working with:
- Searching for assets
- Finding assets by path
- Opening assets in editors
- Duplicating assets
- Saving assets
- Deleting assets
- Importing textures
- Exporting textures
- Getting asset dependencies
- Getting asset referencers

## Core Services

### AssetDiscoveryService
Asset operations:
- Search assets by name pattern
- Find asset by exact path
- Get assets by type
- List assets in directory
- Open asset in editor
- Duplicate asset
- Save asset(s)
- Delete asset
- Import/export textures
- Get dependencies and referencers

## Quick Examples

### Search for Assets
```python
import unreal

# Search by name
results = unreal.AssetDiscoveryService.search_assets("Player", "Blueprint")
for asset in results:
    print(f"{asset.name}: {asset.path}")
```

### Find Specific Asset
```python
import unreal

# Check if asset exists
asset = unreal.AssetDiscoveryService.find_asset_by_path("/Game/BP_Player")
if asset:
    print(f"Found: {asset.name}")
else:
    print("Asset not found")
```

### Duplicate Asset
```python
import unreal

unreal.AssetDiscoveryService.duplicate_asset(
    "/Game/BP_Enemy",
    "/Game/BP_Boss"
)
```

### Save Assets
```python
import unreal

# Save specific asset
unreal.AssetDiscoveryService.save_asset("/Game/BP_Player")

# Save all dirty assets
count = unreal.AssetDiscoveryService.save_all_assets()
print(f"Saved {count} assets")
```

### Import Texture
```python
import unreal

unreal.AssetDiscoveryService.import_texture(
    "C:/Textures/character.png",
    "/Game/Textures/T_Character"
)
```

## Related Skills

This skill is commonly used alongside other skills to find assets before operating on them:
- **blueprints**: Find blueprints before modifying
- **materials**: Find materials before editing
- **data-tables**: Find Data Tables before adding rows
- etc.
