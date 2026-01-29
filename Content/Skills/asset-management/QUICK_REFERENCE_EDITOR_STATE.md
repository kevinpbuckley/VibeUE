# Quick Reference: Asset Editor State & Content Browser Queries

## Methods Overview

| Method | Returns | Description |
|--------|---------|-------------|
| `get_open_assets()` | `TArray<FAssetData>` | All assets currently open in editors |
| `is_asset_open(path)` | `bool` | Check if specific asset is open |
| `get_content_browser_selections()` | `TArray<FAssetData>` | All assets selected in Content Browser |
| `get_primary_content_browser_selection(out)` | `bool` | First selected asset (+ out param) |

---

## Quick Examples

### Check What's Open
```python
import unreal
open_assets = unreal.AssetDiscoveryService.get_open_assets()
print(f"Editing {len(open_assets)} assets")
```

### Check Specific Asset
```python
import unreal
if unreal.AssetDiscoveryService.is_asset_open("/Game/BP_Player"):
    print("BP_Player is open")
```

### Get Selected Assets
```python
import unreal
selected = unreal.AssetDiscoveryService.get_content_browser_selections()
for asset in selected:
    print(asset.asset_name)
```

### Get Primary Selection
```python
import unreal
asset = unreal.AssetData()
if unreal.AssetDiscoveryService.get_primary_content_browser_selection(asset):
    print(f"Selected: {asset.asset_name}")
```

---

## Common Patterns

### Process Selected Blueprints
```python
import unreal

selected = unreal.AssetDiscoveryService.get_content_browser_selections()
for asset in selected:
    if "Blueprint" in str(asset.asset_class_path):
        print(f"Blueprint: {asset.asset_name}")
        # Do something with blueprint
```

### Avoid Editing Open Assets
```python
import unreal

def safe_to_modify(asset_path):
    return not unreal.AssetDiscoveryService.is_asset_open(asset_path)

if safe_to_modify("/Game/BP_Enemy"):
    # Modify asset
    pass
```

### Open Primary Selection
```python
import unreal

asset = unreal.AssetData()
if unreal.AssetDiscoveryService.get_primary_content_browser_selection(asset):
    path = asset.package_path.to_string()
    unreal.AssetDiscoveryService.open_asset(path)
```

---

## FAssetData Properties

When you get assets, you can access:

```python
asset = unreal.AssetData()
# ... get asset via one of the methods ...

# Common properties (UE 5.7+):
asset.asset_name          # FName - asset name (e.g., "BP_Player")
asset.package_path        # FName - full path (e.g., "/Game/Blueprints/BP_Player")
asset.asset_class_path    # FTopLevelAssetPath - class type info
asset.package_name        # FName - package name

# Convert to string:
str(asset.asset_name)
asset.package_path.to_string()
```

---

## Notes

- All methods are editor-only (require GEditor)
- Returns empty arrays/false when nothing found (no exceptions)
- Thread-safe with null checks
- Works in Python via UAssetDiscoveryService exposure
