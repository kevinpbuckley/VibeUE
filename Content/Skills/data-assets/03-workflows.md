# Data Asset Workflows

Common patterns for working with Data Assets.

---

## Create Data Asset with Properties

```python
import unreal
import json

# 1. Search for available Data Asset classes
classes = unreal.DataAssetService.search_asset_classes("Item")
for c in classes:
    print(f"{c.name}: {c.path}")

# 2. Create Data Asset
asset_path = unreal.DataAssetService.create_data_asset(
    "UItemDataAsset",          # Class name
    "/Game/Data/Items/",       # Path
    "DA_HealthPotion"          # Name
)

# 3. Set properties individually
unreal.DataAssetService.set_property(asset_path, "Name", "Health Potion")
unreal.DataAssetService.set_property(asset_path, "Description", "Restores 50 health")
unreal.DataAssetService.set_property(asset_path, "Value", "50")
unreal.DataAssetService.set_property(asset_path, "Price", "10")
unreal.DataAssetService.set_property(asset_path, "IsConsumable", "true")

# 4. Or set multiple properties at once
properties = {
    "Name": "Health Potion",
    "Description": "Restores 50 health",
    "Value": "50",
    "Price": "10",
    "IsConsumable": "true"
}
unreal.DataAssetService.set_properties(asset_path, json.dumps(properties))

# 5. Set asset reference
unreal.DataAssetService.set_property(
    asset_path,
    "Icon",
    "/Game/Textures/Items/T_HealthPotion.T_HealthPotion"
)

# 6. Set array property
tags = ["Consumable", "Health", "Potion"]
unreal.DataAssetService.set_array_property(asset_path, "Tags", json.dumps(tags))

# 7. Save
unreal.EditorAssetLibrary.save_asset(asset_path)

print(f"Created: {asset_path}")
```

---

## Inspect Data Asset

```python
import unreal
import json

asset_path = "/Game/Data/Items/DA_HealthPotion"

# 1. Get asset info
info = unreal.DataAssetService.get_info(asset_path)
if info:
    print(f"Asset: {info.name}")
    print(f"Class: {info.asset_class}")
    print(f"Property count: {info.property_count}")

# 2. Get all properties
props = unreal.DataAssetService.get_properties(asset_path)
print("\nProperties:")
for prop in props:
    print(f"  {prop.name}: {prop.type}")
    print(f"    Category: {prop.category}")
    print(f"    Editable: {prop.editable}")

# 3. Get simple property values
name = unreal.DataAssetService.get_property(asset_path, "Name")
value = unreal.DataAssetService.get_property(asset_path, "Value")
print(f"\n{name}: Value={value}")

# 4. Get struct property
stats_json = unreal.DataAssetService.get_struct_property(asset_path, "Stats")
if stats_json:
    stats = json.loads(stats_json)
    print(f"Stats: {stats}")

# 5. Get array property
tags_json = unreal.DataAssetService.get_array_property(asset_path, "Tags")
if tags_json:
    tags = json.loads(tags_json)
    print(f"Tags: {tags}")
```

---

## Clone and Modify Data Asset

```python
import unreal
import json

source_path = "/Game/Data/Items/DA_HealthPotion"
dest_path = "/Game/Data/Items/DA_SuperHealthPotion"

# 1. Duplicate the asset
unreal.AssetDiscoveryService.duplicate_asset(source_path, dest_path)

# 2. Modify properties
unreal.DataAssetService.set_property(dest_path, "Name", "Super Health Potion")
unreal.DataAssetService.set_property(dest_path, "Description", "Restores 200 health")
unreal.DataAssetService.set_property(dest_path, "Value", "200")
unreal.DataAssetService.set_property(dest_path, "Price", "50")

# 3. Modify struct property
stats_json = unreal.DataAssetService.get_struct_property(dest_path, "Stats")
if stats_json:
    stats = json.loads(stats_json)
    stats["HealAmount"] = 200  # Increase heal amount
    unreal.DataAssetService.set_struct_property(dest_path, "Stats", json.dumps(stats))

# 4. Add to array
unreal.DataAssetService.add_array_element(dest_path, "Tags", "Powerful")

# 5. Save
unreal.EditorAssetLibrary.save_asset(dest_path)

print(f"Created modified copy: {dest_path}")
```

---

## Create Multiple Data Assets from Template

```python
import unreal
import json

# Template data
base_weapon = {
    "Type": "Weapon",
    "IsEquippable": "true",
    "Weight": "5.0"
}

# Weapon definitions
weapons = [
    {"name": "Sword", "damage": "50", "price": "100", "mesh": "SM_Sword"},
    {"name": "Axe", "damage": "75", "price": "150", "mesh": "SM_Axe"},
    {"name": "Bow", "damage": "40", "price": "120", "mesh": "SM_Bow"},
    {"name": "Spear", "damage": "60", "price": "110", "mesh": "SM_Spear"}
]

# Create assets
for weapon in weapons:
    # 1. Create Data Asset
    asset_path = unreal.DataAssetService.create_data_asset(
        "UWeaponDataAsset",
        "/Game/Data/Weapons/",
        f"DA_{weapon['name']}"
    )

    # 2. Set base properties
    unreal.DataAssetService.set_properties(asset_path, json.dumps(base_weapon))

    # 3. Set specific properties
    unreal.DataAssetService.set_property(asset_path, "Name", weapon['name'])
    unreal.DataAssetService.set_property(asset_path, "Damage", weapon['damage'])
    unreal.DataAssetService.set_property(asset_path, "Price", weapon['price'])

    # 4. Set mesh reference
    mesh_path = f"/Game/Meshes/Weapons/{weapon['mesh']}.{weapon['mesh']}"
    unreal.DataAssetService.set_property(asset_path, "Mesh", mesh_path)

    # 5. Save
    unreal.EditorAssetLibrary.save_asset(asset_path)

    print(f"Created: {asset_path}")
```

---

## Update Multiple Data Assets

```python
import unreal
import json

# 1. Find all weapon Data Assets
assets = unreal.DataAssetService.list_data_assets(path_filter="/Game/Data/Weapons")

print(f"Found {len(assets)} weapon assets")

# 2. Update each asset
for asset in assets:
    # Get current damage
    damage_str = unreal.DataAssetService.get_property(asset.path, "Damage")
    if damage_str:
        # Increase damage by 10%
        damage = int(damage_str)
        new_damage = int(damage * 1.1)

        unreal.DataAssetService.set_property(asset.path, "Damage", str(new_damage))
        print(f"{asset.name}: Damage {damage} -> {new_damage}")

        # Save
        unreal.EditorAssetLibrary.save_asset(asset.path)
```

---

## Validate Data Asset Properties

```python
import unreal
import json

asset_path = "/Game/Data/Items/DA_Sword"

# Get all properties
props = unreal.DataAssetService.get_properties(asset_path)

# Validate each property
errors = []

for prop in props:
    value = unreal.DataAssetService.get_property(asset_path, prop.name)

    # Check if value is set
    if not value or value == "":
        errors.append(f"Property '{prop.name}' is empty")
        continue

    # Custom validation based on property name
    if prop.name == "Damage":
        try:
            damage = int(value)
            if damage < 0:
                errors.append(f"Damage cannot be negative: {damage}")
            elif damage > 1000:
                errors.append(f"Damage seems too high: {damage}")
        except ValueError:
            errors.append(f"Damage is not a valid number: {value}")

    if prop.name == "Price":
        try:
            price = int(value)
            if price < 0:
                errors.append(f"Price cannot be negative: {price}")
        except ValueError:
            errors.append(f"Price is not a valid number: {value}")

    if prop.name == "Weight":
        try:
            weight = float(value)
            if weight <= 0:
                errors.append(f"Weight must be positive: {weight}")
        except ValueError:
            errors.append(f"Weight is not a valid number: {value}")

# Report results
if errors:
    print(f"\nValidation errors for {asset_path}:")
    for error in errors:
        print(f"  - {error}")
else:
    print(f"\n{asset_path} is valid!")
```

---

## Populate Data Asset from External Data

```python
import unreal
import json

# External data (could be from CSV, JSON file, database, etc.)
item_definitions = [
    {
        "id": "sword_iron",
        "name": "Iron Sword",
        "type": "Weapon",
        "rarity": "Common",
        "stats": {"damage": 50, "speed": 1.0},
        "price": 100,
        "mesh": "/Game/Meshes/SM_IronSword.SM_IronSword"
    },
    {
        "id": "sword_steel",
        "name": "Steel Sword",
        "type": "Weapon",
        "rarity": "Uncommon",
        "stats": {"damage": 75, "speed": 1.1},
        "price": 200,
        "mesh": "/Game/Meshes/SM_SteelSword.SM_SteelSword"
    }
]

# Create Data Assets from external data
for item in item_definitions:
    # 1. Create asset
    asset_name = f"DA_{item['id']}"
    asset_path = unreal.DataAssetService.create_data_asset(
        "UItemDataAsset",
        "/Game/Data/Items/",
        asset_name
    )

    # 2. Set simple properties
    unreal.DataAssetService.set_property(asset_path, "ID", item['id'])
    unreal.DataAssetService.set_property(asset_path, "Name", item['name'])
    unreal.DataAssetService.set_property(asset_path, "Type", item['type'])
    unreal.DataAssetService.set_property(asset_path, "Rarity", item['rarity'])
    unreal.DataAssetService.set_property(asset_path, "Price", str(item['price']))
    unreal.DataAssetService.set_property(asset_path, "Mesh", item['mesh'])

    # 3. Set struct property
    unreal.DataAssetService.set_struct_property(
        asset_path,
        "Stats",
        json.dumps(item['stats'])
    )

    # 4. Save
    unreal.EditorAssetLibrary.save_asset(asset_path)

    print(f"Created: {asset_name}")
```

---

## Export Data Assets to JSON

```python
import unreal
import json

# 1. Find all Data Assets in a path
assets = unreal.DataAssetService.list_data_assets(path_filter="/Game/Data/Items")

# 2. Export each asset
exported = {}

for asset in assets:
    asset_data = {
        "name": asset.name,
        "class": asset.asset_class,
        "path": asset.path,
        "properties": {}
    }

    # Get all properties
    props = unreal.DataAssetService.get_properties(asset.path)

    for prop in props:
        # Get simple property
        value = unreal.DataAssetService.get_property(asset.path, prop.name)

        if prop.type == "Struct":
            # Get struct as JSON
            struct_json = unreal.DataAssetService.get_struct_property(asset.path, prop.name)
            if struct_json:
                asset_data["properties"][prop.name] = json.loads(struct_json)
        elif prop.type == "Array":
            # Get array as JSON
            array_json = unreal.DataAssetService.get_array_property(asset.path, prop.name)
            if array_json:
                asset_data["properties"][prop.name] = json.loads(array_json)
        else:
            # Simple property
            asset_data["properties"][prop.name] = value

    exported[asset.name] = asset_data

# Print or save
print(json.dumps(exported, indent=2))

# Or write to file (if file system access available)
# with open("exported_assets.json", "w") as f:
#     json.dump(exported, f, indent=2)
```

---

## Compare Two Data Assets

```python
import unreal
import json

asset1_path = "/Game/Data/Items/DA_Sword"
asset2_path = "/Game/Data/Items/DA_Axe"

# Get properties for both assets
props1 = unreal.DataAssetService.get_properties(asset1_path)
props2 = unreal.DataAssetService.get_properties(asset2_path)

# Compare property values
differences = []

for prop in props1:
    value1 = unreal.DataAssetService.get_property(asset1_path, prop.name)
    value2 = unreal.DataAssetService.get_property(asset2_path, prop.name)

    if value1 != value2:
        differences.append({
            "property": prop.name,
            "asset1": value1,
            "asset2": value2
        })

# Report differences
print(f"Comparing {asset1_path} vs {asset2_path}:\n")
if differences:
    for diff in differences:
        print(f"{diff['property']}:")
        print(f"  Asset 1: {diff['asset1']}")
        print(f"  Asset 2: {diff['asset2']}")
else:
    print("Assets are identical")
```

---

## Batch Update Property Across Assets

```python
import unreal

# Find all item Data Assets
assets = unreal.DataAssetService.list_data_assets(class_filter="ItemDataAsset")

print(f"Updating {len(assets)} items...")

# Add new property to all assets
for asset in assets:
    # Add "IsNew" flag
    unreal.DataAssetService.set_property(asset.path, "IsNew", "false")

    # Add "Version" property
    unreal.DataAssetService.set_property(asset.path, "Version", "1")

    # Save
    unreal.EditorAssetLibrary.save_asset(asset.path)

    print(f"Updated: {asset.name}")

print("Batch update complete!")
```

---

## Create Data Asset Hierarchy

```python
import unreal
import json

# Create base weapon Data Asset
base_weapon_path = unreal.DataAssetService.create_data_asset(
    "UWeaponDataAsset",
    "/Game/Data/Weapons/",
    "DA_BaseWeapon"
)

# Set base properties
base_properties = {
    "Type": "Weapon",
    "IsEquippable": "true",
    "Weight": "5.0",
    "Durability": "100"
}
unreal.DataAssetService.set_properties(base_weapon_path, json.dumps(base_properties))
unreal.EditorAssetLibrary.save_asset(base_weapon_path)

# Create specific weapons that reference base
sword_path = unreal.DataAssetService.create_data_asset(
    "UWeaponDataAsset",
    "/Game/Data/Weapons/",
    "DA_Sword"
)

# Copy properties from base
base_props = unreal.DataAssetService.get_properties(base_weapon_path)
for prop in base_props:
    value = unreal.DataAssetService.get_property(base_weapon_path, prop.name)
    if value:
        unreal.DataAssetService.set_property(sword_path, prop.name, value)

# Override specific properties
unreal.DataAssetService.set_property(sword_path, "Name", "Iron Sword")
unreal.DataAssetService.set_property(sword_path, "Damage", "50")
unreal.DataAssetService.set_property(sword_path, "Price", "100")

# Save
unreal.EditorAssetLibrary.save_asset(sword_path)

print(f"Created weapon hierarchy")
```

---

## Common Patterns Summary

### Pattern 1: Create → Configure → Save
```python
import unreal
import json

# Create
path = unreal.DataAssetService.create_data_asset("UItemDataAsset", "/Game/Data/", "DA_Item")

# Configure
properties = {"Name": "Item", "Value": "10"}
unreal.DataAssetService.set_properties(path, json.dumps(properties))

# Save
unreal.EditorAssetLibrary.save_asset(path)
```

### Pattern 2: Check Exists Before Creating
```python
import unreal

path = "/Game/Data/DA_Item"
existing = unreal.AssetDiscoveryService.find_asset_by_path(path)
if not existing:
    unreal.DataAssetService.create_data_asset("UItemDataAsset", "/Game/Data/", "DA_Item")
```

### Pattern 3: Read → Modify → Write
```python
import unreal
import json

# Read
stats_json = unreal.DataAssetService.get_struct_property(path, "Stats")
stats = json.loads(stats_json)

# Modify
stats["Damage"] += 10

# Write
unreal.DataAssetService.set_struct_property(path, "Stats", json.dumps(stats))
```

### Pattern 4: Always Convert to Strings
```python
import unreal

# WRONG: unreal.DataAssetService.set_property(path, "Damage", 75)
# CORRECT:
unreal.DataAssetService.set_property(path, "Damage", "75")
```

### Pattern 5: Use JSON for Complex Types
```python
import unreal
import json

# Structs
location = {"X": 100.0, "Y": 200.0, "Z": 50.0}
unreal.DataAssetService.set_struct_property(path, "Location", json.dumps(location))

# Arrays
tags = ["Weapon", "Melee"]
unreal.DataAssetService.set_array_property(path, "Tags", json.dumps(tags))
```
