---
name: data-tables
display_name: Data Tables
description: Create and modify Data Tables and manage their rows (DataTableService). Use when the user asks to create a Data Table (DT_) from a row struct, add/edit/remove rows, or read row data. Pair with enum-struct to define the row struct first.
vibeue_classes:
  - DataTableService
unreal_classes:
  - EditorAssetLibrary
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "data-driven-design"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Data Tables Skill

## Critical Rules

### 🚨 Row struct names: use exactly what `search_row_types` returns

`create_data_table` **fails silently** — it returns an **empty string** (no exception) when the
row struct can't be resolved. The reason (`CreateDataTable: Row struct not found`) only appears in
the editor log: `read_logs(action="filter", file="main", pattern="DataTableService")`.

```python
structs = unreal.DataTableService.search_row_types("Item")
# Pass s.name EXACTLY as returned — e.g. "GameplayTagTableRow", not "FGameplayTagTableRow"
table_path = unreal.DataTableService.create_data_table(structs[0].name, "/Game/Data", "DT_Items")
if not table_path:
    # resolution failed — check the editor log, do NOT assume the table exists
    ...
```

Both native `FTableRowBase`-derived structs and **UserDefinedStructs** (created with
`EnumStructService.create_struct`) are accepted. User-defined struct columns are addressed by
their **authored names** ("Damage"), the same names shown in the struct editor.

### 🚨 Writes are lenient — ALWAYS read the row back

`add_row` returns `True` even when some (or all) values failed to apply — unknown property
names and unconvertible values are logged as warnings and the row is created with defaults.
After any `add_row`/`update_row`, call `get_row` and compare before claiming success.

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

Use enum VALUE name (not qualified). Works for both `enum class` and `TEnumAsByte` columns,
and `get_row` returns the name string back:

```python
{"ItemType": "Weapon", "Rarity": "Epic"}  # NOT "EItemType::Weapon"
```

An invalid enum name leaves the column at its default — the row is still created
(see "Writes are lenient" above), so verify with `get_row`.

### ⚠️ `list_data_tables` has NO name filter

`list_data_tables(row_struct_filter, path_filter)` — the first argument filters by **row
struct name** (e.g. `"GameplayTagTableRow"`), not by table name. Passing a table-name
pattern like `"DT_Test"` returns 0 results even when the tables exist. To find tables by
name, list with `("", "/Game/YourPath")` and match `info.name` yourself, or use
`EditorAssetLibrary.does_asset_exist(table_path)`.

### ⚠️ Return Type Property Names

| Type | WRONG | CORRECT |
|------|-------|---------|
| RowStructTypeInfo | `info.struct_name` | `info.name` |
| DataTableInfo | `info.asset_name` | `info.name` |
| DataTableDetailedInfo | `info.columns` | `info.columns_json` (parse with `json.loads`) |
| RowStructColumnInfo | `col.column_name` / `col.column_type` | `col.name` / `col.type` (also `cpp_type`, `editable`) |

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

# Create table — pass the struct name EXACTLY as search_row_types returned it
table_path = unreal.DataTableService.create_data_table(structs[0].name, "/Game/Data", "DT_Items")
if not table_path:
    raise RuntimeError("create_data_table failed — check editor log for 'Row struct not found'")
unreal.EditorAssetLibrary.save_asset(table_path)
```

No matching struct? Create one first (load the **enum-struct** skill):
`EnumStructService.create_struct("/Game/Data", "ItemData")` — then create the table with
`"ItemData"` (or `"FItemData"`; the asset is saved with an F prefix and both resolve).

### Add Rows

```python
import json
import unreal

table_path = "/Game/Data/DT_Items"

# Get row struct schema first
columns = unreal.DataTableService.get_row_struct(table_path)
for col in columns:
    print(f"{col.name}: {col.type}")

# Add row, then VERIFY — add_row returns True even if values failed to apply
data = {"Name": "Iron Sword", "Damage": 50, "Price": 100}
unreal.DataTableService.add_row(table_path, "Sword_Iron", json.dumps(data))
print(unreal.DataTableService.get_row(table_path, "Sword_Iron"))  # confirm values stuck
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

## Sample scripts (run via `execute_python_code`)

- **`scripts/create_data_table.pyx`** — create a Data Table from a row struct and add rows.
