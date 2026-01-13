# Data Asset Workflows

Common patterns for working with Data Assets.

---

## Basic: Create → Configure → Save

```python
import unreal
import json

# 1. Create the asset
path = unreal.DataAssetService.create_data_asset("InputAction", "/Game/Data/", "DA_NewItem")

# 2. Configure properties (always use strings!)
unreal.DataAssetService.set_property(path, "Name", "New Item")
unreal.DataAssetService.set_property(path, "Value", "100")
unreal.DataAssetService.set_property(path, "IsActive", "true")

# 3. Save
unreal.EditorAssetLibrary.save_asset(path)
```

---

## Discover Before Creating

Always check what classes and properties exist:

```python
import unreal

# 1. Find available DataAsset classes
types = unreal.DataAssetService.search_types("Item")
for t in types:
    print(f"{t.name}: {t.base_class}")

# 2. Get class schema (properties)
info = unreal.DataAssetService.get_class_info("InputAction", True)
print(f"Properties for {info.name}:")
for p in info.properties:
    print(f"  {p.name}: {p.type}")
```

---

## Check Before Create

```python
import unreal

path = "/Game/Data/DA_Item"

# Check if asset exists
existing = unreal.EditorAssetLibrary.does_asset_exist(path)
if not existing:
    unreal.DataAssetService.create_data_asset("InputAction", "/Game/Data/", "DA_Item")
else:
    print(f"Asset already exists: {path}")
```

---

## Inspect Existing Asset

```python
import unreal

path = "/Game/Data/DA_Item"

# Get info with all properties
info = unreal.DataAssetService.get_info(path)
print(f"Asset: {info.name}")
print(f"Class: {info.asset_class}")
print(f"Properties: {info.properties_json}")

# Or list properties individually  
props = unreal.DataAssetService.list_properties(path)
for p in props:
    value = unreal.DataAssetService.get_property(path, p.name)
    print(f"{p.name} ({p.type}): {value}")
```

---

## Read → Modify → Write

```python
import unreal
import json

path = "/Game/Data/DA_Item"

# Read struct property
stats_json = unreal.DataAssetService.get_struct_property(path, "Stats")
stats = json.loads(stats_json) if stats_json else {}

# Modify
stats["Damage"] = stats.get("Damage", 0) + 10

# Write back
unreal.DataAssetService.set_struct_property(path, "Stats", json.dumps(stats))
unreal.EditorAssetLibrary.save_asset(path)
```

---

## Set Multiple Properties at Once

```python
import unreal
import json

path = "/Game/Data/DA_Item"

properties = {
    "Name": "Iron Sword",
    "Damage": "50",
    "Weight": "5.5",
    "IsLegendary": "false"
}

result = unreal.DataAssetService.set_properties(path, json.dumps(properties))
# result.successful_properties - list of properties that were set
# result.failed_properties - list of properties that failed
```

---

## Clone and Modify

```python
import unreal

source_path = "/Game/Data/DA_Item"
dest_path = "/Game/Data/DA_Item_Copy"

# Duplicate
unreal.AssetDiscoveryService.duplicate_asset(source_path, dest_path)

# Modify the copy
unreal.DataAssetService.set_property(dest_path, "Name", "Modified Item")
unreal.EditorAssetLibrary.save_asset(dest_path)
```

---

## Batch Update

```python
import unreal

# Find all assets of a type
assets = unreal.DataAssetService.list_data_assets(class_name="InputAction", search_path="/Game/Data")

for asset_path in assets:
    # Update each asset
    unreal.DataAssetService.set_property(asset_path, "Version", "2.0")
    unreal.EditorAssetLibrary.save_asset(asset_path)
```

---

## Error Handling Pattern

```python
import unreal

path = "/Game/Data/DA_Item"

# Check properties exist before setting
props = unreal.DataAssetService.list_properties(path)
prop_names = [p.name for p in props]

if "CustomProperty" in prop_names:
    unreal.DataAssetService.set_property(path, "CustomProperty", "value")
else:
    print("Property CustomProperty doesn't exist on this asset class")
```
