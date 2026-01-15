# Data Table Workflows


---

## Create Data Table

```python
import unreal

# 1. Find available row struct types
structs = unreal.DataTableService.search_row_types("Item")
for s in structs:
    print(f"{s.name}: {s.path}")  # Use .name not .struct_name

# 2. Create table with chosen struct
table_path = unreal.DataTableService.create_data_table(
    "FItemData",          # Row struct name
    "/Game/Data/",        # Destination folder
    "DT_Items"            # Asset name
)

# 3. Save
unreal.EditorAssetLibrary.save_asset(table_path)
```

---

## Add Rows

```python
import json
import unreal

table_path = "/Game/Data/DT_Items"

# Get row struct schema first
columns = unreal.DataTableService.get_row_struct(table_path)
for col in columns:
    print(f"{col.column_name}: {col.column_type}")  # Use .column_name not .name

# Add row with correct properties
data = {
    "Name": "Iron Sword",
    "Damage": 50,
    "Price": 100
}
unreal.DataTableService.add_row(table_path, "Sword_Iron", json.dumps(data))

# Save
unreal.EditorAssetLibrary.save_asset(table_path)
```

---

## Update Row

```python
import json
import unreal

# Get current row data
current = unreal.DataTableService.get_row("/Game/DT_Items", "Sword_Iron")
print(f"Current: {current}")

# Update row
new_data = {
    "Name": "Iron Sword +1",
    "Damage": 75,
    "Price": 200
}
unreal.DataTableService.update_row("/Game/DT_Items", "Sword_Iron", json.dumps(new_data))

# Save
unreal.EditorAssetLibrary.save_asset("/Game/DT_Items")
```

---

## List and Query Rows

```python
import unreal
import json

# Get table info (returns DataTableDetailedInfo)
info = unreal.DataTableService.get_info("/Game/DT_Items")
print(f"Table: {info.name}")
print(f"Row count: {info.row_count}")
print(f"Rows: {list(info.row_names)}")

# Parse columns_json (it's a JSON string, not a list!)
columns = json.loads(info.columns_json)
for col in columns:
    print(f"  Column: {col['name']} ({col['type']})")

# List rows (returns Array[str])
row_names = unreal.DataTableService.list_rows("/Game/DT_Items")
for name in row_names:
    # Get each row as JSON string
    row_data = unreal.DataTableService.get_row("/Game/DT_Items", name)
    print(f"{name}: {row_data}")
```

---

## List Data Tables

```python
import unreal

# List all tables in /Game (returns Array[DataTableInfo])
tables = unreal.DataTableService.list_data_tables("", "/Game")
for t in tables:
    print(f"{t.name}: {t.path} (Struct: {t.row_struct})")  # Use .name not .asset_name
    print(f"  Row Count: {t.row_count}")
```

---

## Delete Row

```python
import unreal

unreal.DataTableService.remove_row("/Game/DT_Items", "Sword_Iron")
unreal.EditorAssetLibrary.save_asset("/Game/DT_Items")
```
