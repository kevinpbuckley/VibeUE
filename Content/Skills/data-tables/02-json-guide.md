# Data Table JSON Format Guide

When working with Data Tables, all row data must be provided as JSON strings using `json.dumps()`.

---

## Basic Types

### Strings
```python
import json

data = {
    "Name": "Iron Sword",
    "Description": "A basic iron sword"
}
json.dumps(data)
# Output: '{"Name": "Iron Sword", "Description": "A basic iron sword"}'
```

### Numbers
```python
import json

data = {
    "Health": 100,          # Integer
    "Speed": 5.5,           # Float
    "Damage": 50.0,         # Float
    "Level": 1              # Integer
}
json.dumps(data)
```

### Booleans
**IMPORTANT**: JSON uses lowercase `true`/`false`, not Python's `True`/`False`

```python
import json

data = {
    "IsEquippable": True,   # Python bool
    "IsQuestItem": False
}
json.dumps(data)  # Converts to '{"IsEquippable": true, "IsQuestItem": false}'
```

---

## Asset References

Asset properties use Unreal's asset path format.

### Static Mesh
```python
import json

data = {
    "Mesh": "/Game/Meshes/Weapons/SM_Sword.SM_Sword"
}
json.dumps(data)
```

### Texture
```python
import json

data = {
    "Icon": "/Game/Textures/Items/T_Sword_Icon.T_Sword_Icon"
}
json.dumps(data)
```

### Material
```python
import json

data = {
    "Material": "/Game/Materials/M_Weapon.M_Weapon"
}
json.dumps(data)
```

### Blueprint Class
```python
import json

data = {
    "ActorClass": "/Game/Blueprints/BP_Enemy.BP_Enemy_C"  # Note: _C suffix for class
}
json.dumps(data)
```

---

## Enums

Enums are represented as strings with the enum value name.

```python
import json

# Example: EItemRarity enum with values: Common, Uncommon, Rare, Epic, Legendary
data = {
    "Rarity": "Rare",           # Just the enum value name
    "DamageType": "Physical"    # Another enum example
}
json.dumps(data)
```

---

## Structs

Structs are nested JSON objects.

### FVector
```python
import json

data = {
    "Location": {
        "X": 100.0,
        "Y": 200.0,
        "Z": 50.0
    }
}
json.dumps(data)
```

### FRotator
```python
import json

data = {
    "Rotation": {
        "Pitch": 0.0,
        "Yaw": 90.0,
        "Roll": 0.0
    }
}
json.dumps(data)
```

### FLinearColor
```python
import json

data = {
    "Color": {
        "R": 1.0,
        "G": 0.5,
        "B": 0.0,
        "A": 1.0
    }
}
json.dumps(data)
```

### Custom Struct
```python
import json

# Example: FItemStats struct
data = {
    "Stats": {
        "Damage": 50,
        "Defense": 20,
        "Weight": 5.0,
        "Durability": 100
    }
}
json.dumps(data)
```

---

## Arrays

Arrays use JSON array syntax `[]`.

### Array of Strings
```python
import json

data = {
    "Tags": ["Weapon", "Melee", "Iron"],
    "Categories": ["Combat", "Equipment"]
}
json.dumps(data)
```

### Array of Numbers
```python
import json

data = {
    "DamageLevels": [10, 20, 30, 40, 50],
    "DropRates": [0.1, 0.25, 0.5, 0.75, 1.0]
}
json.dumps(data)
```

### Array of Structs
```python
import json

data = {
    "RequiredItems": [
        {"ItemName": "Iron Ore", "Quantity": 5},
        {"ItemName": "Wood", "Quantity": 2}
    ]
}
json.dumps(data)
```

### Array of Asset References
```python
import json

data = {
    "Materials": [
        "/Game/Materials/M_Metal.M_Metal",
        "/Game/Materials/M_Leather.M_Leather"
    ]
}
json.dumps(data)
```

---

## Complex Example

Complete item data with multiple types:

```python
import json
import unreal

# Complete item row
item_data = {
    # Basic properties
    "Name": "Legendary Sword",
    "Description": "A powerful sword forged from dragon scales",
    "Level": 50,
    "Price": 10000,
    "Weight": 8.5,
    "IsQuestItem": False,
    "IsEquippable": True,

    # Enum
    "Rarity": "Legendary",
    "DamageType": "Slashing",

    # Struct
    "Stats": {
        "Damage": 100,
        "AttackSpeed": 1.2,
        "CritChance": 0.15,
        "CritMultiplier": 2.0
    },

    # Arrays
    "Tags": ["Weapon", "Melee", "Legendary"],
    "EquipSlots": ["RightHand", "LeftHand"],

    # Asset references
    "Mesh": "/Game/Meshes/Weapons/SM_LegendarySword.SM_LegendarySword",
    "Icon": "/Game/Textures/Items/T_LegendarySword_Icon.T_LegendarySword_Icon",
    "Materials": [
        "/Game/Materials/M_DragonScale.M_DragonScale",
        "/Game/Materials/M_Enchanted.M_Enchanted"
    ],

    # Nested struct with array
    "Effects": {
        "OnHit": ["FireDamage", "Stun"],
        "OnEquip": ["StrengthBoost"],
        "Magnitude": 25.0
    }
}

# Add row to Data Table
unreal.DataTableService.add_row(
    "/Game/Data/DT_Items",
    "LegendarySword",
    json.dumps(item_data)
)
```

---

## Common Mistakes

### ❌ WRONG: Passing Python Dict Directly
```python
import unreal

data = {"Name": "Sword"}
unreal.DataTableService.add_row("/Game/DT_Items", "Sword", data)  # TypeError!
```

### ✅ CORRECT: Convert to JSON String
```python
import unreal
import json

data = {"Name": "Sword"}
unreal.DataTableService.add_row("/Game/DT_Items", "Sword", json.dumps(data))
```

---

### ❌ WRONG: Python Boolean Strings
```python
import json

data = {"IsActive": "True"}  # String "True" is not a boolean!
json.dumps(data)  # {"IsActive": "True"}  (wrong)
```

### ✅ CORRECT: Python Boolean Values
```python
import json

data = {"IsActive": True}  # Python bool
json.dumps(data)  # {"IsActive": true}  (correct JSON)
```

---

### ❌ WRONG: Missing Asset Path Extension
```python
import json

data = {"Mesh": "/Game/Meshes/Cube"}  # Missing .Cube
json.dumps(data)
```

### ✅ CORRECT: Full Asset Path
```python
import json

data = {"Mesh": "/Game/Meshes/Cube.Cube"}  # Full path with extension
json.dumps(data)
```

---

### ❌ WRONG: Enum as Integer
```python
import json

data = {"Rarity": 2}  # Don't use enum index!
json.dumps(data)
```

### ✅ CORRECT: Enum as String Name
```python
import json

data = {"Rarity": "Rare"}  # Use enum value name
json.dumps(data)
```

---

## Discovering Struct Fields

If you don't know the fields of a struct, use `get_row_struct()`:

```python
import unreal

# Get struct schema for a Data Table
columns = unreal.DataTableService.get_row_struct("/Game/Data/DT_Items")

for col in columns:
    print(f"{col.name}: {col.type}")
    print(f"  C++ Type: {col.cpp_type}")
    print(f"  Editable: {col.editable}")
```

Or get from struct name directly:

```python
import unreal

columns = unreal.DataTableService.get_row_struct("FItemData")
for col in columns:
    print(f"{col.name}: {col.type}")
```

---

## Partial Updates

You can update only specific fields without providing the entire row:

```python
import unreal
import json

# Only update Damage field
updates = {"Damage": 75}
unreal.DataTableService.update_row(
    "/Game/Data/DT_Items",
    "Sword",
    json.dumps(updates)
)

# Update multiple fields
updates = {
    "Damage": 75,
    "Price": 150,
    "IsEquippable": True
}
unreal.DataTableService.update_row(
    "/Game/Data/DT_Items",
    "Sword",
    json.dumps(updates)
)
```

---

## Bulk Operations

When adding many rows, use `add_rows()` for better performance:

```python
import unreal
import json

rows = {
    "Sword": {
        "Name": "Iron Sword",
        "Damage": 50,
        "Price": 100
    },
    "Axe": {
        "Name": "Battle Axe",
        "Damage": 75,
        "Price": 150
    },
    "Bow": {
        "Name": "Long Bow",
        "Damage": 40,
        "Price": 120
    }
}

result = unreal.DataTableService.add_rows(
    "/Game/Data/DT_Items",
    json.dumps(rows)
)

print(f"Succeeded: {list(result.succeeded_rows)}")
print(f"Failed: {list(result.failed_rows)}")
if result.failed_rows:
    print(f"Reasons: {list(result.failed_reasons)}")
```

---

## Reading Rows

Always parse JSON when reading rows:

```python
import unreal
import json

row_json = unreal.DataTableService.get_row("/Game/Data/DT_Items", "Sword")
if row_json:
    row_data = json.loads(row_json)  # Parse JSON string to Python dict
    print(f"Name: {row_data['Name']}")
    print(f"Damage: {row_data['Damage']}")
    print(f"Price: {row_data['Price']}")
```
