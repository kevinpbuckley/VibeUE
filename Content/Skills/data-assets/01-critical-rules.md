# DataAssetService - Critical Rules

This file contains gotchas and rules that API discovery doesn't tell you.

---

## ⚠️ Property Values Are ALWAYS Strings

All property values must be passed as strings, even for numbers and booleans.

```python
# WRONG - will cause TypeError
unreal.DataAssetService.set_property(path, "Damage", 75)
unreal.DataAssetService.set_property(path, "IsActive", True)

# CORRECT
unreal.DataAssetService.set_property(path, "Damage", "75")
unreal.DataAssetService.set_property(path, "IsActive", "true")
```

---

## ⚠️ Complex Properties Use Unreal String Format, NOT JSON

For arrays of structs, use Unreal's T3D-like syntax:

```python
# WRONG - JSON format won't work
keys_json = '[{"EntryName": "Key1"}, {"EntryName": "Key2"}]'

# CORRECT - Unreal string format
keys_str = '((EntryName="Key1"),(EntryName="Key2"))'
unreal.DataAssetService.set_property(path, "Keys", keys_str)
```

Simple arrays CAN use JSON when using set_array_property (if available).

---

## ⚠️ Asset Reference Format

Asset references require the full path WITH the asset name repeated:

```python
# WRONG
"/Game/Meshes/Cube"

# CORRECT  
"/Game/Meshes/Cube.Cube"
```

For Blueprint classes, add `_C` suffix:
```python
"/Game/Blueprints/BP_Enemy.BP_Enemy_C"
```

---

## ⚠️ Return Types Are Structs, Not Dicts

Service methods return typed structs. Property names come from auto-discovery.

```python
# WRONG - trying to access like dict
info = unreal.DataAssetService.get_info(path)
print(info["name"])  # ERROR!

# CORRECT - access as properties
print(info.name)
print(info.asset_class)
```

---

## ⚠️ Always List Properties First

Don't guess property names - they vary by DataAsset class:

```python
# First: see what properties exist
props = unreal.DataAssetService.list_properties(asset_path)
for p in props:
    print(f"{p.name}: {p.type}")

# Then: set only properties that exist
unreal.DataAssetService.set_property(path, "ValidPropertyName", "value")
```

---

## ⚠️ search_types Returns Only Concrete Classes

The `search_types()` method filters out abstract classes like `DataAsset` and `PrimaryDataAsset`. Only classes that can actually be instantiated appear.

---

## ⚠️ Always Save After Modifications

Changes are NOT automatically saved:

```python
unreal.DataAssetService.set_property(path, "Damage", "100")

# MUST call save
unreal.EditorAssetLibrary.save_asset(path)
```
