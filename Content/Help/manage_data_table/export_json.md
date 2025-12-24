# export_json

Export all rows from a data table as JSON.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | Yes | Path to the data table asset |
| `format` | string | No | `"object"` (default) or `"array"` |

## Returns (Object Format - Default)

```json
{
  "success": true,
  "table_path": "/Game/Data/Tables/DT_Items",
  "row_struct": "FItemTableRow",
  "data": {
    "Item_Sword": {
      "ItemName": "Iron Sword",
      "Price": 100,
      "Damage": 25
    },
    "Item_Shield": {
      "ItemName": "Wooden Shield",
      "Price": 50,
      "Defense": 10
    }
  }
}
```

## Returns (Array Format)

```json
{
  "success": true,
  "table_path": "/Game/Data/Tables/DT_Items",
  "row_struct": "FItemTableRow",
  "data": [
    {
      "_row_name": "Item_Sword",
      "ItemName": "Iron Sword",
      "Price": 100,
      "Damage": 25
    },
    {
      "_row_name": "Item_Shield",
      "ItemName": "Wooden Shield",
      "Price": 50,
      "Defense": 10
    }
  ]
}
```

## Examples

```python
# Export as object (default)
manage_data_table(
    action="export_json",
    table_path="/Game/Data/Tables/DT_Items"
)

# Export as array
manage_data_table(
    action="export_json",
    table_path="/Game/Data/Tables/DT_Items",
    format="array"
)
```

## Format Comparison

| Format | Best For |
|--------|----------|
| `object` | Import back to UE, lookup by row name |
| `array` | External tools, spreadsheet conversion, iteration |

## Workflow: Backup and Restore

```python
# 1. Export (backup)
result = manage_data_table(
    action="export_json",
    table_path="/Game/Data/Tables/DT_Items"
)

# Store result["data"] somewhere...

# 2. Later: Restore
manage_data_table(
    action="clear_rows",
    table_path="/Game/Data/Tables/DT_Items",
    confirm=True
)
manage_data_table(
    action="import_json",
    table_path="/Game/Data/Tables/DT_Items",
    json_data=stored_data
)
```

## Use Cases

1. **Backup**: Export before making changes
2. **Version Control**: Export to track data changes in git
3. **External Editing**: Export, edit in spreadsheet, import back
4. **Data Migration**: Move data between tables or projects
5. **Documentation**: Generate data documentation

## Notes

- All row data is serialized to JSON-compatible format
- Complex types (vectors, colors, etc.) are converted to objects
- Asset references are exported as path strings
- The object format is directly compatible with `import_json`
- For large tables, this may return a large response

## Related Actions

- `import_json`: Import the exported data
- `get_info`: Alternative that includes structure info
- `get_row`: Export single row
