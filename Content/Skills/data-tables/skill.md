---
name: data-tables
display_name: Data Tables
description: Create and modify Data Tables with row management
vibeue_classes:
  - DataTableService
unreal_classes:
  - EditorAssetLibrary
---

# Data Tables Skill

## Critical Rules

### ⚠️ All Values Are JSON Strings

```python
import json

data = {"Name": "Iron Sword", "Damage": 50, "IsEquippable": True}
unreal.DataTableService.add_row("/Game/DT_Items", "Sword_01", json.dumps(data))
```

### ⚠️ Asset Reference Format

```python
{"Mesh": "/Game/Meshes/SM_Sword.SM_Sword",  # .AssetName suffix
 "ActorClass": "/Game/BP_Enemy.BP_Enemy_C"}  # _C for blueprint class
```

### ⚠️ Struct Types (FVector, FLinearColor)

Use Unreal format strings, NOT JSON objects:

```python
{"Location": "(X=100.0,Y=200.0,Z=0.0)",
 "Color": "(R=1.0,G=0.0,B=0.0,A=1.0)"}
```

### ⚠️ Enum Values

Use enum VALUE name (not qualified):

```python
{"ItemType": "Weapon", "Rarity": "Epic"}  # NOT "EItemType::Weapon"
```

### ⚠️ Return Type Property Names

| Type | WRONG | CORRECT |
|------|-------|---------|
| RowStructTypeInfo | `info.struct_name` | `info.name` |
| DataTableInfo | `info.asset_name` | `info.name` |
| DataTableDetailedInfo | `info.columns` | `info.columns_json` (parse with `json.loads`) |
| RowStructColumnInfo | `col.name` | `col.column_name` |

### ⚠️ Save After Modify

```python
unreal.DataTableService.add_row(table_path, row_name, json_data)
unreal.EditorAssetLibrary.save_asset(table_path)  # REQUIRED
```

---

## Workflows

### Create Data Table

```python
import unreal

# Find available row struct types
structs = unreal.DataTableService.search_row_types("Item")
for s in structs:
    print(f"{s.name}: {s.path}")

# Create table
table_path = unreal.DataTableService.create_data_table("FItemData", "/Game/Data/", "DT_Items")
unreal.EditorAssetLibrary.save_asset(table_path)
```

### Add Rows

```python
import json
import unreal

table_path = "/Game/Data/DT_Items"

# Get row struct schema first
columns = unreal.DataTableService.get_row_struct(table_path)
for col in columns:
    print(f"{col.column_name}: {col.column_type}")

# Add row
data = {"Name": "Iron Sword", "Damage": 50, "Price": 100}
unreal.DataTableService.add_row(table_path, "Sword_Iron", json.dumps(data))
unreal.EditorAssetLibrary.save_asset(table_path)
```

### List and Query Rows

```python
import unreal
import json

info = unreal.DataTableService.get_info("/Game/DT_Items")
print(f"Row count: {info.row_count}")

# Parse columns_json (it's a JSON string!)
columns = json.loads(info.columns_json)
for col in columns:
    print(f"{col['name']}: {col['type']}")

# List rows
row_names = unreal.DataTableService.list_rows("/Game/DT_Items")
for name in row_names:
    row_data = unreal.DataTableService.get_row("/Game/DT_Items", name)
    print(f"{name}: {row_data}")
```

### Update Row

```python
import json
import unreal

new_data = {"Name": "Iron Sword +1", "Damage": 75, "Price": 200}
unreal.DataTableService.update_row("/Game/DT_Items", "Sword_Iron", json.dumps(new_data))
unreal.EditorAssetLibrary.save_asset("/Game/DT_Items")
```

### Delete Row

```python
import unreal

unreal.DataTableService.remove_row("/Game/DT_Items", "Sword_Iron")
unreal.EditorAssetLibrary.save_asset("/Game/DT_Items")
```
