# DataAssetService API Reference

All methods are called via `unreal.DataAssetService.<method_name>(...)`.

**ALWAYS use `discover_python_class("unreal.DataAssetService")` for parameter details before calling.**

---

## Discovery Methods

### search_asset_classes(search_filter="", base_class="", max_results=50)
Search for available Data Asset classes (UPrimaryDataAsset and derived classes).

**Returns:** Array[AssetClassInfo] with properties: `.name`, `.path`, `.base_class`, `.is_native`

**Example:**
```python
import unreal

# Search for all Data Asset classes
classes = unreal.DataAssetService.search_asset_classes()
for c in classes:
    print(f"{c.name}: {c.path}")
    print(f"  Base: {c.base_class}")

# Search for specific class
item_classes = unreal.DataAssetService.search_asset_classes("Item")
for c in item_classes:
    print(f"{c.name}: {c.path}")
```

### list_data_assets(class_filter="", path_filter="", max_results=100)
List all Data Assets in the project, optionally filtered by class or path.

**Returns:** Array[DataAssetInfo] with properties: `.name`, `.path`, `.asset_class`, `.asset_class_path`

**Example:**
```python
import unreal

# List all Data Assets
assets = unreal.DataAssetService.list_data_assets()
for a in assets:
    print(f"{a.name}: {a.asset_class}")
    print(f"  Path: {a.path}")

# Filter by class
weapon_assets = unreal.DataAssetService.list_data_assets(class_filter="WeaponData")

# Filter by path
game_assets = unreal.DataAssetService.list_data_assets(path_filter="/Game/Data")
```

---

## Lifecycle

### create_data_asset(asset_class, asset_path, asset_name)
Create a new Data Asset instance.

**Returns:** Asset path (str)

**Example:**
```python
import unreal

# Create Data Asset from custom class
asset_path = unreal.DataAssetService.create_data_asset(
    "UItemDataAsset",          # C++ class name or Blueprint path
    "/Game/Data/Items/",       # Directory
    "DA_Sword"                 # Asset name
)
print(f"Created: {asset_path}")

# Create from Blueprint class
asset_path = unreal.DataAssetService.create_data_asset(
    "/Game/Blueprints/BP_WeaponData.BP_WeaponData_C",
    "/Game/Data/Weapons/",
    "DA_IronSword"
)
```

### delete_data_asset(asset_path)
Delete a Data Asset.

**Returns:** bool

**Example:**
```python
import unreal

success = unreal.DataAssetService.delete_data_asset("/Game/Data/DA_OldItem")
if success:
    print("Asset deleted")
```

---

## Info Methods

### get_info(asset_path)
Get detailed Data Asset information.

**Returns:** DataAssetDetailedInfo or None with properties: `.name`, `.path`, `.asset_class`, `.asset_class_path`, `.property_count`, `.properties_json`

**Example:**
```python
import unreal

info = unreal.DataAssetService.get_info("/Game/Data/DA_Sword")
if info:
    print(f"Asset: {info.name}")
    print(f"Class: {info.asset_class}")
    print(f"Property count: {info.property_count}")
    print(f"Properties: {info.properties_json}")
```

### get_class(asset_path)
Get the class of a Data Asset.

**Returns:** str (class name)

**Example:**
```python
import unreal

class_name = unreal.DataAssetService.get_class("/Game/Data/DA_Sword")
print(f"Class: {class_name}")
```

### get_properties(asset_path)
Get all property names and types for a Data Asset.

**Returns:** Array[PropertyInfo] with properties: `.name`, `.type`, `.cpp_type`, `.category`, `.editable`

**Example:**
```python
import unreal

props = unreal.DataAssetService.get_properties("/Game/Data/DA_Sword")
for prop in props:
    print(f"{prop.name}: {prop.type}")
    print(f"  Category: {prop.category}")
    print(f"  Editable: {prop.editable}")
```

---

## Property Operations

### get_property(asset_path, property_name)
Get a property value from a Data Asset.

**Returns:** str (property value as string) or None

**Example:**
```python
import unreal

# Get simple property
damage = unreal.DataAssetService.get_property("/Game/Data/DA_Sword", "Damage")
print(f"Damage: {damage}")

# Get complex property
mesh = unreal.DataAssetService.get_property("/Game/Data/DA_Sword", "Mesh")
print(f"Mesh: {mesh}")
```

### set_property(asset_path, property_name, value)
Set a property value on a Data Asset.

**Returns:** bool

**Example:**
```python
import unreal

# Set simple value
unreal.DataAssetService.set_property("/Game/Data/DA_Sword", "Damage", "75")

# Set boolean
unreal.DataAssetService.set_property("/Game/Data/DA_Sword", "IsLegendary", "true")

# Set float
unreal.DataAssetService.set_property("/Game/Data/DA_Sword", "Weight", "5.5")

# Set string
unreal.DataAssetService.set_property("/Game/Data/DA_Sword", "Description", "A powerful iron sword")

# Set asset reference
unreal.DataAssetService.set_property("/Game/Data/DA_Sword", "Mesh", "/Game/Meshes/SM_Sword.SM_Sword")

# Set enum
unreal.DataAssetService.set_property("/Game/Data/DA_Sword", "Rarity", "Rare")
```

### set_properties(asset_path, properties_json)
Set multiple properties at once using JSON.

**Parameters:**
- `properties_json`: JSON string of property name/value pairs

**Returns:** bool

**Example:**
```python
import unreal
import json

properties = {
    "Damage": "100",
    "Weight": "8.5",
    "IsLegendary": "true",
    "Description": "Legendary Dragon Blade",
    "Rarity": "Legendary"
}

success = unreal.DataAssetService.set_properties(
    "/Game/Data/DA_DragonSword",
    json.dumps(properties)
)
print(f"Properties set: {success}")
```

---

## Struct Properties

### get_struct_property(asset_path, property_name)
Get a struct property as JSON string.

**Returns:** JSON string or None

**Example:**
```python
import unreal
import json

# Get struct property
stats_json = unreal.DataAssetService.get_struct_property("/Game/Data/DA_Sword", "Stats")
if stats_json:
    stats = json.loads(stats_json)
    print(f"Attack: {stats['Attack']}")
    print(f"Defense: {stats['Defense']}")
```

### set_struct_property(asset_path, property_name, struct_json)
Set a struct property using JSON.

**Parameters:**
- `struct_json`: JSON string representing the struct

**Returns:** bool

**Example:**
```python
import unreal
import json

# Set FVector struct
location = {
    "X": 100.0,
    "Y": 200.0,
    "Z": 50.0
}

unreal.DataAssetService.set_struct_property(
    "/Game/Data/DA_SpawnPoint",
    "Location",
    json.dumps(location)
)

# Set custom struct
stats = {
    "Attack": 75,
    "Defense": 50,
    "Speed": 1.5,
    "CritChance": 0.15
}

unreal.DataAssetService.set_struct_property(
    "/Game/Data/DA_Weapon",
    "Stats",
    json.dumps(stats)
)
```

---

## Array Properties

### get_array_property(asset_path, property_name)
Get an array property as JSON string.

**Returns:** JSON string (array) or None

**Example:**
```python
import unreal
import json

# Get array property
tags_json = unreal.DataAssetService.get_array_property("/Game/Data/DA_Sword", "Tags")
if tags_json:
    tags = json.loads(tags_json)
    print(f"Tags: {tags}")
```

### set_array_property(asset_path, property_name, array_json)
Set an array property using JSON.

**Parameters:**
- `array_json`: JSON string representing the array

**Returns:** bool

**Example:**
```python
import unreal
import json

# Set array of strings
tags = ["Weapon", "Melee", "Sword", "Iron"]
unreal.DataAssetService.set_array_property(
    "/Game/Data/DA_Sword",
    "Tags",
    json.dumps(tags)
)

# Set array of numbers
damage_levels = [10, 20, 30, 40, 50]
unreal.DataAssetService.set_array_property(
    "/Game/Data/DA_Weapon",
    "DamageLevels",
    json.dumps(damage_levels)
)

# Set array of asset references
materials = [
    "/Game/Materials/M_Metal.M_Metal",
    "/Game/Materials/M_Leather.M_Leather"
]
unreal.DataAssetService.set_array_property(
    "/Game/Data/DA_Armor",
    "Materials",
    json.dumps(materials)
)
```

### add_array_element(asset_path, property_name, element_value)
Add an element to an array property.

**Returns:** bool

**Example:**
```python
import unreal

# Add string to array
unreal.DataAssetService.add_array_element(
    "/Game/Data/DA_Sword",
    "Tags",
    "Legendary"
)

# Add number to array
unreal.DataAssetService.add_array_element(
    "/Game/Data/DA_Weapon",
    "DamageLevels",
    "60"
)
```

### remove_array_element(asset_path, property_name, index)
Remove an element from an array property by index.

**Returns:** bool

**Example:**
```python
import unreal

# Remove element at index 2
unreal.DataAssetService.remove_array_element(
    "/Game/Data/DA_Sword",
    "Tags",
    2
)
```

### clear_array_property(asset_path, property_name)
Clear all elements from an array property.

**Returns:** bool

**Example:**
```python
import unreal

unreal.DataAssetService.clear_array_property("/Game/Data/DA_Sword", "Tags")
```

---

## Validation

### validate_property_value(asset_path, property_name, value)
Validate if a value is valid for a property (without setting it).

**Returns:** ValidationResult with `.is_valid` (bool) and `.error_message` (str)

**Example:**
```python
import unreal

result = unreal.DataAssetService.validate_property_value(
    "/Game/Data/DA_Sword",
    "Damage",
    "-50"  # Invalid negative damage
)

if result.is_valid:
    print("Value is valid")
else:
    print(f"Invalid: {result.error_message}")
```

---

## Critical Rules

### Property Values Are Strings

All property values must be passed as strings, even for numbers and booleans.

**WRONG:**
```python
unreal.DataAssetService.set_property(path, "Damage", 75)  # TypeError!
unreal.DataAssetService.set_property(path, "IsActive", True)  # TypeError!
```

**CORRECT:**
```python
unreal.DataAssetService.set_property(path, "Damage", "75")
unreal.DataAssetService.set_property(path, "IsActive", "true")
```

### Struct and Array Properties Use JSON

For complex types, use `json.dumps()`:

**CORRECT:**
```python
import json

# Struct
location = {"X": 100.0, "Y": 200.0, "Z": 50.0}
unreal.DataAssetService.set_struct_property(path, "Location", json.dumps(location))

# Array
tags = ["Weapon", "Melee"]
unreal.DataAssetService.set_array_property(path, "Tags", json.dumps(tags))
```

### Always Save After Modifications

```python
import unreal

# Modify properties
unreal.DataAssetService.set_property(path, "Damage", "100")

# MUST save
unreal.EditorAssetLibrary.save_asset(path)
```

### Asset Reference Format

Asset references require full path with extension:

**WRONG:**
```python
unreal.DataAssetService.set_property(path, "Mesh", "/Game/Meshes/Cube")
```

**CORRECT:**
```python
unreal.DataAssetService.set_property(path, "Mesh", "/Game/Meshes/Cube.Cube")
```

### Use Return Structs Correctly

Service methods return structs, not dicts:

**WRONG:**
```python
assets = unreal.DataAssetService.list_data_assets()
for a in assets:
    info = unreal.DataAssetService.get_info(a)  # ERROR! a is struct
```

**CORRECT:**
```python
assets = unreal.DataAssetService.list_data_assets()
for a in assets:
    info = unreal.DataAssetService.get_info(a.path)  # Use .path property
    print(f"{a.name}: {a.asset_class}")
```
