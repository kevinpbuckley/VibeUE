---
name: data-assets
display_name: Data Assets
description: Create and modify Primary Data Assets with property management
vibeue_classes:
  - DataAssetService
unreal_classes:
  - EditorAssetLibrary
---

# Data Assets Skill

## Critical Rules

### ⚠️ Property Values Are ALWAYS Strings

```python
# WRONG
unreal.DataAssetService.set_property(path, "Damage", 75)

# CORRECT
unreal.DataAssetService.set_property(path, "Damage", "75")
unreal.DataAssetService.set_property(path, "IsActive", "true")
```

### ⚠️ Complex Properties Use Unreal String Format, NOT JSON

```python
# WRONG - JSON format won't work
keys_json = '[{"EntryName": "Key1"}]'

# CORRECT - Unreal string format
keys_str = '((EntryName="Key1"),(EntryName="Key2"))'
```

### ⚠️ Asset Reference Format

```python
# WRONG
"/Game/Meshes/Cube"

# CORRECT - repeat asset name
"/Game/Meshes/Cube.Cube"

# Blueprint classes add _C suffix
"/Game/Blueprints/BP_Enemy.BP_Enemy_C"
```

### ⚠️ Return Types Are Structs, Not Dicts

```python
# WRONG
info = unreal.DataAssetService.get_info(path)
print(info["name"])  # ERROR!

# CORRECT
print(info.name)
print(info.class_name)
```

### ⚠️ Always Save After Modifications

```python
unreal.DataAssetService.set_property(path, "Damage", "100")
unreal.EditorAssetLibrary.save_asset(path)  # REQUIRED
```

---

## Property Formats

### Simple Types

```python
unreal.DataAssetService.set_property(path, "Level", "50")        # Integer
unreal.DataAssetService.set_property(path, "Weight", "5.5")      # Float
unreal.DataAssetService.set_property(path, "IsActive", "true")   # Boolean
unreal.DataAssetService.set_property(path, "Name", "Iron Sword") # String
```

### Structs (Unreal Text Format)

```python
# FVector
unreal.DataAssetService.set_property(path, "Location", "(X=100.0,Y=200.0,Z=50.0)")

# FLinearColor
unreal.DataAssetService.set_property(path, "Color", "(R=1.0,G=0.5,B=0.0,A=1.0)")

# Custom struct
unreal.DataAssetService.set_property(path, "Stats", "(Attack=75,Defense=50)")
```

### Arrays of Structs

```python
# Use Unreal syntax: ((field=value),(field=value))
items_str = '((Name="Sword",Quantity=1),(Name="Potion",Quantity=5))'
unreal.DataAssetService.set_property(path, "Inventory", items_str)
```

---

## Workflows

### Create → Configure → Save

```python
import unreal

path = unreal.DataAssetService.create_data_asset("InputAction", "/Game/Data/", "DA_NewItem")
unreal.DataAssetService.set_property(path, "Name", "New Item")
unreal.DataAssetService.set_property(path, "Value", "100")
unreal.EditorAssetLibrary.save_asset(path)
```

### Discover Properties First

```python
import unreal

# Find available DataAsset classes
types = unreal.DataAssetService.search_types("Item")
for t in types:
    print(f"{t.name}: {t.parent_class}")

# Get class schema
info = unreal.DataAssetService.get_class_info("InputAction", True)
for p in info.properties:
    print(f"  {p.name}: {p.type}")
```

### Inspect Existing Asset

```python
import unreal

path = "/Game/Data/DA_Item"
props = unreal.DataAssetService.list_properties(path)
for p in props:
    value = unreal.DataAssetService.get_property(path, p.name)
    print(f"{p.name}: {value}")
```

### Set Multiple Properties

```python
import unreal
import json

properties = {"Name": "Iron Sword", "Damage": "50", "Weight": "5.5"}
result = unreal.DataAssetService.set_properties(path, json.dumps(properties))
# result.success_properties - list of properties set
# result.failed_properties - list that failed
```
