# DataAsset Critical Rules

**Note:** Method signatures are in `vibeue_apis` from skill loader. This file contains gotchas that discovery can't tell you.

---

## 📋 Return Type Properties

Quick reference for common return type properties:

### DataAssetTypeInfo
```python
type_info.name          # str - Class name (e.g., "InputAction")
type_info.parent_class  # str - Parent class name
```

### DataAssetInstanceInfo  
```python
info.name         # str - Asset name
info.path         # str - Full asset path
info.class_name   # str - Asset's class name
info.properties   # Array[DataAssetPropertyInfo]
```

### DataAssetPropertyInfo
```python
prop.name       # str - Property name
prop.type       # str - Property type
prop.value      # str - Current value (Unreal text format)
```

### DataAssetSetPropertiesResult
```python
result.success_properties  # Array[str] - Properties that succeeded
result.failed_properties   # Array[str] - Properties that failed
# NOTE: No 'success' boolean flag
```

### DataAssetClassInfo
```python
class_info.class_name   # str - Class name
class_info.parent_class # str - Parent class
class_info.properties   # Array[DataAssetPropertyInfo]
```

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

The DataAssetService exposes only `set_property` and `set_properties`. Use Unreal text format for structs, arrays, and maps.

---

## ⚠️ `create_data_asset` Signature + Return Value

`create_data_asset` takes **class name first**, then path, then name, and returns the **asset path string** (empty on failure):

```python
path = unreal.DataAssetService.create_data_asset("InputAction", "/Game/Data", "IA_Test")
if not path:
    print("Create failed")
```

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
print(info.class_name)
```

---

## ⚠️ `set_properties` Result Fields

The result has `success_properties` and `failed_properties` (no `success` flag).

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
