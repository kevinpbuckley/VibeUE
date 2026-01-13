# Data Table Critical Rules

---

## ⚠️ CRITICAL: All Values Are JSON Strings

Row data must be JSON. Use `json.dumps()`:

```python
import json
import unreal

data = {
    "Name": "Iron Sword",
    "Damage": 50,
    "IsEquippable": True  # Python bool → JSON true
}

unreal.DataTableService.add_row("/Game/DT_Items", "Sword_01", json.dumps(data))
```

---

## ⚠️ CRITICAL: Asset Reference Format

Asset paths need the full path WITH asset name suffix:

```python
{
    "Mesh": "/Game/Meshes/SM_Sword.SM_Sword",  # .AssetName suffix
    "Icon": "/Game/Textures/T_Icon.T_Icon",
    "ActorClass": "/Game/BP_Enemy.BP_Enemy_C"  # _C for blueprint class
}
```

---

## ⚠️ CRITICAL: Struct Types (FVector, FLinearColor, etc.)

Use Unreal format strings, NOT JSON objects:

```python
{
    "Location": "(X=100.0,Y=200.0,Z=0.0)",         # FVector
    "Rotation": "(Pitch=0.0,Yaw=90.0,Roll=0.0)",  # FRotator
    "Color": "(R=1.0,G=0.0,B=0.0,A=1.0)",         # FLinearColor (0-1)
    "Size": "(X=100.0,Y=50.0)"                     # FVector2D
}
```

---

## ⚠️ CRITICAL: Enum Values

Use enum VALUE name (not qualified):

```python
{
    "ItemType": "Weapon",      # NOT "EItemType::Weapon"
    "Rarity": "Epic"           # Just the value name
}
```

---

## ⚠️ CRITICAL: Arrays

Arrays use JSON array syntax:

```python
{
    "Tags": ["Combat", "Melee", "Rare"],
    "Stats": [10, 20, 30],
    "Materials": [
        "/Game/M_Iron.M_Iron",
        "/Game/M_Steel.M_Steel"
    ]
}
```

---

## ⚠️ CRITICAL: Save After Modify

```python
unreal.DataTableService.add_row(table_path, row_name, json_data)
unreal.EditorAssetLibrary.save_asset(table_path)  # REQUIRED
```

---

## ⚠️ Row Names Must Be Unique

Row names are keys - duplicates overwrite:

```python
# These create ONE row, not two
unreal.DataTableService.add_row(path, "Sword", data1)
unreal.DataTableService.add_row(path, "Sword", data2)  # Overwrites!
```

---

## ⚠️ Check Row Struct Before Adding

Get the struct schema to know required properties:

```python
# Discover what properties the row struct has
columns = unreal.DataTableService.get_row_struct("/Game/DT_Items")
for col in columns:
    print(f"{col.name}: {col.type}")
```
