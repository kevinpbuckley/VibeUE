# import_json

Import rows into a data table from JSON data.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | Yes | Path to the data table asset |
| `json_data` | object | Yes | JSON object with row data (row_name keys) |
| `mode` | string | No | `"merge"` (default) or `"replace"` |

## Returns

```json
{
  "success": true,
  "message": "Imported 5 rows to /Game/Data/Tables/DT_Items",
  "imported_count": 5,
  "mode": "merge",
  "failed_count": 1
}
```

## Examples

```python
# Merge import (adds new, skips existing)
manage_data_table(
    action="import_json",
    table_path="/Game/Data/Tables/DT_Items",
    json_data={
        "Item_NewSword": {"ItemName": "Magic Sword", "Price": 500},
        "Item_NewShield": {"ItemName": "Magic Shield", "Price": 400}
    }
)

# Replace import (overwrites existing)
manage_data_table(
    action="import_json",
    table_path="/Game/Data/Tables/DT_Items",
    json_data={
        "Item_Sword": {"ItemName": "Updated Sword", "Price": 200}
    },
    mode="replace"
)

# Full table replacement
manage_data_table(action="clear_rows", table_path="/Game/Data/Tables/DT_Items", confirm=True)
manage_data_table(
    action="import_json",
    table_path="/Game/Data/Tables/DT_Items",
    json_data={
        "Item_1": {"ItemName": "Sword", "Price": 100},
        "Item_2": {"ItemName": "Shield", "Price": 80},
        "Item_3": {"ItemName": "Potion", "Price": 25}
    }
)
```

## Import Modes

| Mode | Behavior |
|------|----------|
| `merge` | Add new rows, skip existing (default) |
| `replace` | Add new rows, update existing |

## Data Format

The `json_data` should be an object keyed by row names:

```json
{
  "RowName1": {
    "Property1": "value",
    "Property2": 123
  },
  "RowName2": {
    "Property1": "other value",
    "Property2": 456
  }
}
```

## Workflow: Backup and Restore

```python
# 1. Export current data
backup = manage_data_table(
    action="export_json",
    table_path="/Game/Data/Tables/DT_Items"
)

# 2. Make changes...

# 3. Restore from backup
manage_data_table(
    action="clear_rows",
    table_path="/Game/Data/Tables/DT_Items",
    confirm=True
)
manage_data_table(
    action="import_json",
    table_path="/Game/Data/Tables/DT_Items",
    json_data=backup["data"]
)
```

## Errors

| Error | Cause |
|-------|-------|
| `table_path is required` | Missing table_path parameter |
| `json_data is required` | Missing or invalid json_data |
| Per-row errors | Invalid data format for specific rows |

## Notes

- Property values must match the row struct's column types
- Unknown properties are ignored (no error)
- Missing properties use default values
- The table is marked dirty; use `manage_asset(action="save")` to persist

## Related Actions

- `export_json`: Export data for backup
- `add_rows`: Similar but always fails on existing rows
- `clear_rows`: Clear before full import
