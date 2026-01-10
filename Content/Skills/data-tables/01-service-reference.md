# DataTableService API Reference

All methods are called via `unreal.DataTableService.<method_name>(...)`.

**ALWAYS use `discover_python_class("unreal.DataTableService")` for parameter details before calling.**

---

## Discovery Methods

### search_row_types(search_filter="")
Search for available row struct types.

**Returns:** Array[RowStructTypeInfo] with properties: `.name`, `.path`, `.module`, `.parent_struct`, `.is_native`, `.property_names`

**Example:**
```python
import unreal

# Search for item-related structs
structs = unreal.DataTableService.search_row_types("Item")
for s in structs:
    print(f"{s.name}: {s.path}")
    print(f"  Properties: {list(s.property_names)}")
```

### list_data_tables(row_struct_filter="", path_filter="")
List all Data Tables, optionally filtered.

**Returns:** Array[DataTableInfo] with properties: `.name`, `.path`, `.row_struct`, `.row_struct_path`, `.row_count`

**Example:**
```python
import unreal

# List all Data Tables
tables = unreal.DataTableService.list_data_tables()
for t in tables:
    print(f"{t.name}: {t.row_count} rows")
    print(f"  Path: {t.path}")
    print(f"  Struct: {t.row_struct}")

# Filter by path
game_tables = unreal.DataTableService.list_data_tables(path_filter="/Game/Data")
```

---

## Lifecycle

### create_data_table(row_struct_name, asset_path, asset_name)
Create a new Data Table.

**Returns:** Asset path (str)

**Example:**
```python
import unreal

# Create Data Table with FItemData struct
table_path = unreal.DataTableService.create_data_table(
    "FItemData",
    "/Game/Data/",
    "DT_Items"
)
print(f"Created: {table_path}")
```

---

## Info Methods

### get_info(table_path)
Get detailed Data Table information.

**Returns:** DataTableDetailedInfo or None with properties: `.name`, `.path`, `.row_struct`, `.row_struct_path`, `.row_count`, `.row_names`, `.columns_json`

**Example:**
```python
import unreal

info = unreal.DataTableService.get_info("/Game/Data/DT_Items")
if info:
    print(f"Table: {info.name}")
    print(f"Row struct: {info.row_struct}")
    print(f"Row count: {info.row_count}")
    print(f"Rows: {list(info.row_names)}")
    print(f"Columns: {info.columns_json}")
```

### get_row_struct(table_path_or_struct_name)
Get row struct schema.

**Returns:** Array[RowStructColumnInfo] with properties: `.name`, `.type`, `.cpp_type`, `.category`, `.tooltip`, `.editable`

**Example:**
```python
import unreal

# Get from table
columns = unreal.DataTableService.get_row_struct("/Game/Data/DT_Items")
for col in columns:
    print(f"{col.name}: {col.type}")
    print(f"  C++ Type: {col.cpp_type}")
    print(f"  Editable: {col.editable}")

# Or get from struct name directly
columns = unreal.DataTableService.get_row_struct("FItemData")
```

---

## Row Operations

### list_rows(table_path)
Get all row names in a Data Table.

**Returns:** Array[str] of row names

**Example:**
```python
import unreal

rows = unreal.DataTableService.list_rows("/Game/Data/DT_Items")
print(f"Rows: {rows}")
```

### get_row(table_path, row_name)
Get a single row as JSON string.

**Returns:** JSON string (empty string if not found)

**Example:**
```python
import unreal
import json

row_json = unreal.DataTableService.get_row("/Game/Data/DT_Items", "Sword")
if row_json:
    row_data = json.loads(row_json)
    print(f"Sword data: {row_data}")
```

### add_row(table_path, row_name, data_json)
Add a new row to the Data Table.

**Parameters:**
- `data_json`: JSON string (use `json.dumps()`)

**Returns:** bool

**Example:**
```python
import unreal
import json

data = {
    "Name": "Iron Sword",
    "Damage": 50,
    "Price": 100,
    "IsEquippable": True
}

success = unreal.DataTableService.add_row(
    "/Game/Data/DT_Items",
    "Sword",
    json.dumps(data)
)
print(f"Added: {success}")
```

### add_rows(table_path, rows_json)
Add multiple rows at once (bulk operation).

**Parameters:**
- `rows_json`: JSON string of dict mapping row names to row data

**Returns:** BulkRowOperationResult with `.succeeded_rows`, `.failed_rows`, `.failed_reasons`

**Example:**
```python
import unreal
import json

rows = {
    "Sword": {"Name": "Iron Sword", "Damage": 50},
    "Axe": {"Name": "Battle Axe", "Damage": 75},
    "Bow": {"Name": "Long Bow", "Damage": 40}
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

### update_row(table_path, row_name, data_json)
Update an existing row (partial or full update).

**Returns:** bool

**Example:**
```python
import unreal
import json

# Partial update - only change Damage
updates = {"Damage": 60}
unreal.DataTableService.update_row(
    "/Game/Data/DT_Items",
    "Sword",
    json.dumps(updates)
)

# Full update - replace entire row
full_data = {"Name": "Steel Sword", "Damage": 60, "Price": 150}
unreal.DataTableService.update_row(
    "/Game/Data/DT_Items",
    "Sword",
    json.dumps(full_data)
)
```

### remove_row(table_path, row_name)
Remove a row from the Data Table.

**Returns:** bool

**Example:**
```python
import unreal

unreal.DataTableService.remove_row("/Game/Data/DT_Items", "OldItem")
```

### rename_row(table_path, old_name, new_name)
Rename a row.

**Returns:** bool

**Example:**
```python
import unreal

unreal.DataTableService.rename_row(
    "/Game/Data/DT_Items",
    "Sword",
    "IronSword"
)
```

### clear_rows(table_path)
Remove all rows from the Data Table.

**Returns:** int (count of removed rows)

**Example:**
```python
import unreal

count = unreal.DataTableService.clear_rows("/Game/Data/DT_Items")
print(f"Removed {count} rows")
```

---

## Critical Rules

### Return Types Are Structs

**WRONG:**
```python
tables = unreal.DataTableService.list_data_tables()
for t in tables:
    info = unreal.DataTableService.get_info(t)  # ERROR! t is struct
```

**CORRECT:**
```python
tables = unreal.DataTableService.list_data_tables()
for t in tables:
    info = unreal.DataTableService.get_info(t.path)  # Use .path property
    print(f"{t.name}: {t.row_count} rows")
```

### JSON Data Format

**WRONG:**
```python
# Passing Python dict directly
data = {"Name": "Sword"}
unreal.DataTableService.add_row(path, "Sword", data)  # TypeError!
```

**CORRECT:**
```python
import json

# Convert to JSON string first
data = {"Name": "Sword"}
unreal.DataTableService.add_row(path, "Sword", json.dumps(data))
```

---

## Supported Property Types in JSON

- **Strings**: `"Name": "Value"`
- **Numbers**: `"Health": 100`, `"Speed": 1.5`
- **Booleans**: `"IsActive": true` (lowercase in JSON)
- **Asset paths**: `"Mesh": "/Game/Meshes/Cube.Cube"`
- **Enums**: `"Type": "EnumValueName"` (enum name as string)
- **Arrays**: `"Tags": ["Combat", "Melee"]`
- **Nested structs**: `"Location": {"X": 0, "Y": 0, "Z": 0}`
