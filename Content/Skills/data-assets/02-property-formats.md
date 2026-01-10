# Data Asset Property Formats

Guide for formatting complex property values in Data Assets.

---

## Simple Types

All simple values are passed as strings.

### Integer
```python
import unreal

unreal.DataAssetService.set_property("/Game/Data/DA_Item", "Level", "50")
unreal.DataAssetService.set_property("/Game/Data/DA_Item", "Quantity", "100")
```

### Float
```python
import unreal

unreal.DataAssetService.set_property("/Game/Data/DA_Item", "Weight", "5.5")
unreal.DataAssetService.set_property("/Game/Data/DA_Item", "DamageMultiplier", "1.25")
```

### Boolean
Use lowercase "true" or "false":

```python
import unreal

unreal.DataAssetService.set_property("/Game/Data/DA_Item", "IsEquippable", "true")
unreal.DataAssetService.set_property("/Game/Data/DA_Item", "IsQuestItem", "false")
```

### String
```python
import unreal

unreal.DataAssetService.set_property("/Game/Data/DA_Item", "Name", "Iron Sword")
unreal.DataAssetService.set_property("/Game/Data/DA_Item", "Description", "A basic iron sword")
```

### Name (FName)
Same as string:

```python
import unreal

unreal.DataAssetService.set_property("/Game/Data/DA_Item", "ID", "item_sword_001")
```

### Text (FText)
```python
import unreal

unreal.DataAssetService.set_property("/Game/Data/DA_Item", "DisplayName", "Iron Sword")
```

---

## Unreal Struct Types

### FVector
```python
import unreal
import json

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
```

### FRotator
```python
import unreal
import json

rotation = {
    "Pitch": 0.0,
    "Yaw": 90.0,
    "Roll": 0.0
}

unreal.DataAssetService.set_struct_property(
    "/Game/Data/DA_SpawnPoint",
    "Rotation",
    json.dumps(rotation)
)
```

### FTransform
```python
import unreal
import json

transform = {
    "Translation": {"X": 0.0, "Y": 0.0, "Z": 0.0},
    "Rotation": {"X": 0.0, "Y": 0.0, "Z": 0.0, "W": 1.0},  # Quaternion
    "Scale3D": {"X": 1.0, "Y": 1.0, "Z": 1.0}
}

unreal.DataAssetService.set_struct_property(
    "/Game/Data/DA_Actor",
    "Transform",
    json.dumps(transform)
)
```

### FVector2D
```python
import unreal
import json

size = {
    "X": 1920.0,
    "Y": 1080.0
}

unreal.DataAssetService.set_struct_property(
    "/Game/Data/DA_Screen",
    "Resolution",
    json.dumps(size)
)
```

### FLinearColor
```python
import unreal
import json

color = {
    "R": 1.0,
    "G": 0.5,
    "B": 0.0,
    "A": 1.0
}

unreal.DataAssetService.set_struct_property(
    "/Game/Data/DA_Material",
    "BaseColor",
    json.dumps(color)
)
```

### FColor (0-255 range)
```python
import unreal
import json

color = {
    "R": 255,
    "G": 128,
    "B": 0,
    "A": 255
}

unreal.DataAssetService.set_struct_property(
    "/Game/Data/DA_Particle",
    "EmitColor",
    json.dumps(color)
)
```

---

## Asset References

Asset references require full path with extension.

### Static Mesh
```python
import unreal

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Weapon",
    "Mesh",
    "/Game/Meshes/Weapons/SM_Sword.SM_Sword"
)
```

### Skeletal Mesh
```python
import unreal

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Character",
    "SkeletalMesh",
    "/Game/Characters/SK_Player.SK_Player"
)
```

### Material
```python
import unreal

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Weapon",
    "Material",
    "/Game/Materials/M_Weapon.M_Weapon"
)
```

### Material Instance
```python
import unreal

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Weapon",
    "MaterialInstance",
    "/Game/Materials/MI_IronSword.MI_IronSword"
)
```

### Texture
```python
import unreal

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Item",
    "Icon",
    "/Game/Textures/Items/T_Sword_Icon.T_Sword_Icon"
)
```

### Blueprint Class
Blueprint classes require `_C` suffix:

```python
import unreal

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Spawner",
    "ActorClass",
    "/Game/Blueprints/BP_Enemy.BP_Enemy_C"
)
```

### Sound
```python
import unreal

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Weapon",
    "AttackSound",
    "/Game/Audio/A_SwordSwing.A_SwordSwing"
)
```

### Particle System
```python
import unreal

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Weapon",
    "HitEffect",
    "/Game/Effects/P_Impact.P_Impact"
)
```

### Animation Sequence
```python
import unreal

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Character",
    "IdleAnim",
    "/Game/Animations/A_Idle.A_Idle"
)
```

### Other Data Asset
```python
import unreal

unreal.DataAssetService.set_property(
    "/Game/Data/DA_QuestReward",
    "ItemData",
    "/Game/Data/Items/DA_Sword.DA_Sword"
)
```

---

## Enums

Enums are set as string values (enum value name).

### Simple Enum
```python
import unreal

# EItemRarity enum: Common, Uncommon, Rare, Epic, Legendary
unreal.DataAssetService.set_property(
    "/Game/Data/DA_Item",
    "Rarity",
    "Legendary"
)

# EDamageType enum: Physical, Fire, Ice, Lightning
unreal.DataAssetService.set_property(
    "/Game/Data/DA_Weapon",
    "DamageType",
    "Fire"
)
```

### Enum with Namespace
If enum has a namespace prefix, include it:

```python
import unreal

# EItemType::Weapon
unreal.DataAssetService.set_property(
    "/Game/Data/DA_Item",
    "Type",
    "EItemType::Weapon"
)
```

---

## Arrays

Arrays use JSON format.

### Array of Strings
```python
import unreal
import json

tags = ["Weapon", "Melee", "Sword", "Iron"]
unreal.DataAssetService.set_array_property(
    "/Game/Data/DA_Weapon",
    "Tags",
    json.dumps(tags)
)
```

### Array of Numbers
```python
import unreal
import json

damage_per_level = [10, 20, 30, 40, 50]
unreal.DataAssetService.set_array_property(
    "/Game/Data/DA_Weapon",
    "DamageLevels",
    json.dumps(damage_per_level)
)
```

### Array of Booleans
```python
import unreal
import json

flags = [True, False, True, True]
unreal.DataAssetService.set_array_property(
    "/Game/Data/DA_Config",
    "EnabledFeatures",
    json.dumps(flags)
)
```

### Array of Enums
```python
import unreal
import json

damage_types = ["Physical", "Fire", "Ice"]
unreal.DataAssetService.set_array_property(
    "/Game/Data/DA_Weapon",
    "DamageTypes",
    json.dumps(damage_types)
)
```

### Array of Asset References
```python
import unreal
import json

materials = [
    "/Game/Materials/M_Metal.M_Metal",
    "/Game/Materials/M_Leather.M_Leather",
    "/Game/Materials/M_Wood.M_Wood"
]

unreal.DataAssetService.set_array_property(
    "/Game/Data/DA_Armor",
    "Materials",
    json.dumps(materials)
)
```

### Array of Structs
```python
import unreal
import json

# Array of FVector
spawn_points = [
    {"X": 0.0, "Y": 0.0, "Z": 0.0},
    {"X": 100.0, "Y": 0.0, "Z": 0.0},
    {"X": 0.0, "Y": 100.0, "Z": 0.0}
]

unreal.DataAssetService.set_array_property(
    "/Game/Data/DA_Level",
    "SpawnLocations",
    json.dumps(spawn_points)
)

# Array of custom struct
required_items = [
    {"ItemName": "Iron Ore", "Quantity": 5},
    {"ItemName": "Wood", "Quantity": 2},
    {"ItemName": "Leather", "Quantity": 1}
]

unreal.DataAssetService.set_array_property(
    "/Game/Data/DA_Recipe",
    "RequiredItems",
    json.dumps(required_items)
)
```

---

## Custom Structs

Custom structs are nested JSON objects.

### Simple Custom Struct
```python
import unreal
import json

# FItemStats struct
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

### Nested Structs
```python
import unreal
import json

# FCharacterData struct with nested FInventoryInfo
character_data = {
    "Name": "Hero",
    "Level": 10,
    "Inventory": {
        "MaxSlots": 20,
        "CurrentWeight": 45.5,
        "Items": ["Sword", "Potion", "Shield"]
    }
}

unreal.DataAssetService.set_struct_property(
    "/Game/Data/DA_Character",
    "CharacterData",
    json.dumps(character_data)
)
```

### Struct with Asset References
```python
import unreal
import json

# FWeaponVisuals struct
visuals = {
    "Mesh": "/Game/Meshes/SM_Sword.SM_Sword",
    "Material": "/Game/Materials/M_Metal.M_Metal",
    "Icon": "/Game/Textures/T_Sword.T_Sword",
    "TrailEffect": "/Game/Effects/P_Trail.P_Trail"
}

unreal.DataAssetService.set_struct_property(
    "/Game/Data/DA_Weapon",
    "Visuals",
    json.dumps(visuals)
)
```

### Struct with Enum
```python
import unreal
import json

# FItemDefinition struct
definition = {
    "Name": "Legendary Sword",
    "Type": "Weapon",
    "Rarity": "Legendary",
    "Level": 50,
    "Value": 10000
}

unreal.DataAssetService.set_struct_property(
    "/Game/Data/DA_Item",
    "Definition",
    json.dumps(definition)
)
```

---

## Maps (TMap)

Maps are represented as JSON objects (key-value pairs).

### Map of String to Number
```python
import unreal
import json

stat_bonuses = {
    "Strength": 10,
    "Dexterity": 5,
    "Intelligence": 8
}

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Equipment",
    "StatBonuses",
    json.dumps(stat_bonuses)
)
```

### Map of String to String
```python
import unreal
import json

localized_names = {
    "en": "Iron Sword",
    "es": "Espada de Hierro",
    "fr": "Épée de Fer",
    "de": "Eisenschwert"
}

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Item",
    "LocalizedNames",
    json.dumps(localized_names)
)
```

### Map of String to Asset Reference
```python
import unreal
import json

material_overrides = {
    "Handle": "/Game/Materials/M_Wood.M_Wood",
    "Blade": "/Game/Materials/M_Steel.M_Steel",
    "Gem": "/Game/Materials/M_Ruby.M_Ruby"
}

unreal.DataAssetService.set_property(
    "/Game/Data/DA_Weapon",
    "MaterialOverrides",
    json.dumps(material_overrides)
)
```

---

## Complex Example

Complete Data Asset with all property types:

```python
import unreal
import json

# Create Data Asset
asset_path = unreal.DataAssetService.create_data_asset(
    "UWeaponDataAsset",
    "/Game/Data/Weapons/",
    "DA_LegendarySword"
)

# Simple properties
unreal.DataAssetService.set_property(asset_path, "Name", "Legendary Dragon Blade")
unreal.DataAssetService.set_property(asset_path, "Description", "A sword forged from dragon scales")
unreal.DataAssetService.set_property(asset_path, "Level", "50")
unreal.DataAssetService.set_property(asset_path, "Weight", "8.5")
unreal.DataAssetService.set_property(asset_path, "IsLegendary", "true")

# Enum
unreal.DataAssetService.set_property(asset_path, "Rarity", "Legendary")
unreal.DataAssetService.set_property(asset_path, "DamageType", "Fire")

# Asset references
unreal.DataAssetService.set_property(asset_path, "Mesh", "/Game/Meshes/SM_DragonSword.SM_DragonSword")
unreal.DataAssetService.set_property(asset_path, "Icon", "/Game/Textures/T_DragonSword.T_DragonSword")

# Struct
stats = {
    "BaseDamage": 100,
    "AttackSpeed": 1.2,
    "CritChance": 0.25,
    "CritMultiplier": 3.0
}
unreal.DataAssetService.set_struct_property(asset_path, "Stats", json.dumps(stats))

# Array of strings
tags = ["Weapon", "Melee", "Legendary", "Fire"]
unreal.DataAssetService.set_array_property(asset_path, "Tags", json.dumps(tags))

# Array of asset references
materials = [
    "/Game/Materials/M_DragonScale.M_DragonScale",
    "/Game/Materials/M_Fire.M_Fire"
]
unreal.DataAssetService.set_array_property(asset_path, "Materials", json.dumps(materials))

# Nested struct with arrays
effects = {
    "OnHit": ["BurnDamage", "ArmorPenetration"],
    "OnEquip": ["StrengthBoost", "FireResistance"],
    "Magnitude": 50.0,
    "Duration": 10.0
}
unreal.DataAssetService.set_struct_property(asset_path, "Effects", json.dumps(effects))

# Save
unreal.EditorAssetLibrary.save_asset(asset_path)

print(f"Created and configured: {asset_path}")
```

---

## Reading Property Values

Always check property type before parsing:

```python
import unreal
import json

asset_path = "/Game/Data/DA_Weapon"

# Simple property
damage = unreal.DataAssetService.get_property(asset_path, "Damage")
print(f"Damage: {damage}")  # String value

# Struct property
stats_json = unreal.DataAssetService.get_struct_property(asset_path, "Stats")
if stats_json:
    stats = json.loads(stats_json)  # Parse JSON
    print(f"Attack: {stats['Attack']}")

# Array property
tags_json = unreal.DataAssetService.get_array_property(asset_path, "Tags")
if tags_json:
    tags = json.loads(tags_json)  # Parse JSON
    print(f"Tags: {tags}")
```
