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

## Structs (Unreal Text Format)

Use Unreal text format for struct properties (ExportText/ImportText format):

```python
# FVector
unreal.DataAssetService.set_property(path, "Location", "(X=100.0,Y=200.0,Z=50.0)")

# FRotator
unreal.DataAssetService.set_property(path, "Rotation", "(Pitch=0.0,Yaw=90.0,Roll=0.0)")

# FLinearColor
unreal.DataAssetService.set_property(path, "Color", "(R=1.0,G=0.5,B=0.0,A=1.0)")

# Custom struct
unreal.DataAssetService.set_property(path, "Stats", "(Attack=75,Defense=50,CritChance=0.15)")
```

---

## Arrays (Unreal Text Format)

Arrays use Unreal text format:

```python
# Strings
unreal.DataAssetService.set_property(path, "Tags", "(\"Weapon\",\"Melee\",\"Sword\")")

# Numbers
unreal.DataAssetService.set_property(path, "DamageLevels", "(10,20,30,40,50)")

# Asset references
unreal.DataAssetService.set_property(
    path,
    "Materials",
    "(\"/Game/Materials/M_Metal.M_Metal\",\"/Game/Materials/M_Wood.M_Wood\")"
)
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

Maps use Unreal text format. The exact format depends on key/value types, so use
`get_property` or `get_info` to see the correct ExportText string for that property.

```python
unreal.DataAssetService.set_property(
    path,
    "StatBonuses",
    "(\"Strength\"=10,\"Dexterity\"=5,\"Intelligence\"=8)"
)
```

---

## Nested Structs

Nest structs using Unreal text format. Use `get_property` first to copy the exact field names:

```python
unreal.DataAssetService.set_property(
    path,
    "CharacterData",
    "(Name=\"Hero\",Level=10,Stats=(Health=100,Mana=50),Equipment=(Weapon=\"/Game/Data/DA_Sword.DA_Sword\",Armor=\"/Game/Data/DA_Plate.DA_Plate\"))"
)
```

---

## Reading Values

Get values and parse if needed (values are ExportText strings):

```python
# Simple property - returns string
damage = unreal.DataAssetService.get_property(path, "Damage")
damage_int = int(damage)

# Struct/Array/Map - returns Unreal text format string
stats_text = unreal.DataAssetService.get_property(path, "Stats")
tags_text = unreal.DataAssetService.get_property(path, "Tags")
```
