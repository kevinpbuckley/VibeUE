# Data Asset Property Formats

This guide covers how to format property values for DataAssetService methods.

---

## All Values Are Strings

Pass everything as strings - the service parses them:

```python
unreal.DataAssetService.set_property(path, "Level", "50")       # Integer
unreal.DataAssetService.set_property(path, "Weight", "5.5")     # Float
unreal.DataAssetService.set_property(path, "IsActive", "true")  # Boolean (lowercase)
unreal.DataAssetService.set_property(path, "Name", "Iron Sword") # String
```

---

## Asset References

Use full path with asset name repeated. For Blueprint classes, add `_C`:

```python
# Static Mesh
"/Game/Meshes/SM_Sword.SM_Sword"

# Material
"/Game/Materials/M_Metal.M_Metal"

# Texture  
"/Game/Textures/T_Icon.T_Icon"

# Blueprint Class (add _C suffix)
"/Game/Blueprints/BP_Enemy.BP_Enemy_C"

# Data Asset
"/Game/Data/DA_Weapon.DA_Weapon"
```

---

## Enums

Use the enum value name as a string:

```python
unreal.DataAssetService.set_property(path, "Rarity", "Legendary")
unreal.DataAssetService.set_property(path, "DamageType", "Fire")

# With namespace if required
unreal.DataAssetService.set_property(path, "Type", "EItemType::Weapon")
```

---

## Structs (via JSON)

Use JSON for struct properties:

```python
import json

# FVector
location = {"X": 100.0, "Y": 200.0, "Z": 50.0}
unreal.DataAssetService.set_struct_property(path, "Location", json.dumps(location))

# FRotator  
rotation = {"Pitch": 0.0, "Yaw": 90.0, "Roll": 0.0}

# FLinearColor
color = {"R": 1.0, "G": 0.5, "B": 0.0, "A": 1.0}

# Custom struct
stats = {"Attack": 75, "Defense": 50, "CritChance": 0.15}
unreal.DataAssetService.set_struct_property(path, "Stats", json.dumps(stats))
```

---

## Arrays (via JSON)

Simple arrays use JSON:

```python
import json

# Strings
tags = ["Weapon", "Melee", "Sword"]
unreal.DataAssetService.set_array_property(path, "Tags", json.dumps(tags))

# Numbers
damage_levels = [10, 20, 30, 40, 50]
unreal.DataAssetService.set_array_property(path, "DamageLevels", json.dumps(damage_levels))

# Asset references
materials = [
    "/Game/Materials/M_Metal.M_Metal",
    "/Game/Materials/M_Wood.M_Wood"
]
unreal.DataAssetService.set_array_property(path, "Materials", json.dumps(materials))
```

---

## ⚠️ Arrays of Structs - Use Unreal Format

**CRITICAL**: Arrays of structs use Unreal's T3D-like syntax, NOT JSON:

```python
# WRONG - JSON won't work for array of structs
keys_json = '[{"EntryName": "Key1"}, {"EntryName": "Key2"}]'

# CORRECT - Unreal string format
keys_str = '((EntryName="Key1"),(EntryName="Key2"))'
unreal.DataAssetService.set_property(path, "Keys", keys_str)

# More complex example with multiple fields
items_str = '((Name="Sword",Quantity=1,Equipped=true),(Name="Potion",Quantity=5,Equipped=false))'
unreal.DataAssetService.set_property(path, "Inventory", items_str)
```

Format rules for Unreal string syntax:
- Wrap each struct in `()`
- Wrap array in outer `()`
- String values in `"quotes"`
- Numbers without quotes
- Booleans as `true`/`false` without quotes
- Separate fields with `,`

---

## Maps (TMap)

Maps are JSON objects:

```python
import json

stat_bonuses = {
    "Strength": 10,
    "Dexterity": 5,
    "Intelligence": 8
}
unreal.DataAssetService.set_property(path, "StatBonuses", json.dumps(stat_bonuses))
```

---

## Nested Structs

Nest JSON objects for complex structs:

```python
import json

character = {
    "Name": "Hero",
    "Level": 10,
    "Stats": {
        "Health": 100,
        "Mana": 50
    },
    "Equipment": {
        "Weapon": "/Game/Data/DA_Sword.DA_Sword",
        "Armor": "/Game/Data/DA_Plate.DA_Plate"
    }
}
unreal.DataAssetService.set_struct_property(path, "CharacterData", json.dumps(character))
```

---

## Reading Values

Get values and parse if needed:

```python
import json

# Simple property - returns string
damage = unreal.DataAssetService.get_property(path, "Damage")
damage_int = int(damage)

# Struct - returns JSON string
stats_json = unreal.DataAssetService.get_struct_property(path, "Stats")
stats = json.loads(stats_json) if stats_json else {}

# Array - returns JSON string  
tags_json = unreal.DataAssetService.get_array_property(path, "Tags")
tags = json.loads(tags_json) if tags_json else []
```
