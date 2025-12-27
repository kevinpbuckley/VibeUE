# manage_data_table

## Overview

The `manage_data_table` tool provides comprehensive management for UDataTable assets in your Unreal Engine project. It uses reflection to discover available row struct types, create data tables, and perform CRUD operations on table rows.

## Available Actions

| Action | Description | Required Params |
|--------|-------------|-----------------|
| `help` | Get help for this tool or a specific action | - |
| `search_row_types` | Find available row struct types | - |
| `list` | List data tables in project | - |
| `create` | Create a new data table | `row_struct`, `asset_name` |
| `get_info` | Get table structure and rows | `table_path` |
| `get_row_struct` | Get row struct column definitions | `table_path` or `struct_name` |
| `list_rows` | List row names in table | `table_path` |
| `get_row` | Get a single row's data | `table_path`, `row_name` |
| `add_row` | Add a new row | `table_path`, `row_name` |
| `update_row` | Update an existing row | `table_path`, `row_name`, `data` |
| `remove_row` | Remove a row | `table_path`, `row_name` |
| `rename_row` | Rename a row | `table_path`, `row_name`, `new_name` |
| `add_rows` | Bulk add multiple rows | `table_path`, `rows` |
| `clear_rows` | Clear all rows (destructive) | `table_path`, `confirm` |
| `import_json` | Import rows from JSON | `table_path`, `json_data` |
| `export_json` | Export rows to JSON | `table_path` |

**Note:** For delete, duplicate, and save asset operations, use `manage_asset` tool instead.

## Quick Start

### 1. Discover Available Row Struct Types
```python
# Find all row struct types in your project
manage_data_table(action="search_row_types")

# Search for specific types
manage_data_table(action="search_row_types", search_filter="Item")
```

### 2. Create a Data Table
```python
# Create using discovered row struct
manage_data_table(
    action="create",
    row_struct="FItemTableRow",
    asset_path="/Game/Data/Tables",
    asset_name="DT_Items"
)
```

### 3. Add and Modify Rows
```python
# Add a new row
manage_data_table(
    action="add_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_Sword",
    data={
        "Name": "Iron Sword",
        "Damage": 25,
        "Price": 100
    }
)

# Update a row
manage_data_table(
    action="update_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_Sword",
    data={
        "Price": 150
    }
)

# Get row data
manage_data_table(
    action="get_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_Sword"
)
```

### 4. Bulk Operations
```python
# Add multiple rows at once
manage_data_table(
    action="add_rows",
    table_path="/Game/Data/Tables/DT_Items",
    rows={
        "Item_Axe": {"Name": "Iron Axe", "Damage": 30, "Price": 120},
        "Item_Bow": {"Name": "Wooden Bow", "Damage": 15, "Price": 80}
    }
)

# Export all rows to JSON
manage_data_table(
    action="export_json",
    table_path="/Game/Data/Tables/DT_Items"
)

# Import rows from JSON
manage_data_table(
    action="import_json",
    table_path="/Game/Data/Tables/DT_Items",
    json_data={
        "Item_Shield": {"Name": "Iron Shield", "Defense": 20, "Price": 90}
    },
    mode="merge"  # or "replace"
)
```

## Typical Workflow

1. **Discover**: Use `search_row_types` to find available row struct types
2. **Create**: Use `create` to make a new data table with a row struct
3. **Populate**: Use `add_row` or `add_rows` to add data
4. **Query**: Use `get_info`, `list_rows`, or `get_row` to read data
5. **Modify**: Use `update_row`, `remove_row`, or `rename_row` to change data
6. **Backup**: Use `export_json` to export data, `import_json` to restore

## Property Type Support

The tool supports these UE property types:

| Type | JSON Representation |
|------|---------------------|
| bool | `true` / `false` |
| int32, int64 | number |
| float, double | number |
| FString, FName, FText | string |
| FVector | `{"X": 0, "Y": 0, "Z": 0}` |
| FRotator | `{"Pitch": 0, "Yaw": 0, "Roll": 0}` |
| FColor | `{"R": 255, "G": 255, "B": 255, "A": 255}` |
| FLinearColor | `{"R": 1.0, "G": 1.0, "B": 1.0, "A": 1.0}` |
| FSoftObjectPath | string (asset path) |
| TArray | JSON array |
| TMap | JSON object |
| Enum | string (enum value name) |

## Error Handling

The tool returns structured error responses:

```json
{
  "success": false,
  "error": "Row 'Item_Sword' not found in table",
  "error_code": "ROW_NOT_FOUND"
}
```

Common error codes:
- `NOT_FOUND`: Table or row not found
- `ALREADY_EXISTS`: Row already exists
- `INVALID_DATA`: Invalid property data
- `ROW_NOT_FOUND`: Specified row doesn't exist
- `STRUCT_NOT_FOUND`: Row struct type not found

## See Also

- `manage_asset`: For asset operations (delete, duplicate, save)
- `manage_data_asset`: For UDataAsset instances (single-object data)
