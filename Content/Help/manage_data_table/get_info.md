# get_info

Get comprehensive information about a data table including its structure and rows.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | Yes | Path to the data table asset |
| `include_rows` | bool | No | Include row data (default: true) |
| `max_rows` | int | No | Limit number of rows returned (0 = all) |

## Returns

```json
{
  "success": true,
  "name": "DT_Items",
  "path": "/Game/Data/Tables/DT_Items.DT_Items",
  "row_struct": "FItemTableRow",
  "row_struct_path": "/Script/MyGame.ItemTableRow",
  "row_count": 3,
  "columns": [
    {
      "name": "ItemName",
      "type": "String",
      "cpp_type": "FString",
      "category": "",
      "tooltip": "Display name of the item",
      "editable": true
    },
    {
      "name": "Price",
      "type": "Int32",
      "cpp_type": "int32",
      "category": "Economy",
      "tooltip": "",
      "editable": true
    }
  ],
  "rows": {
    "Item_Sword": {
      "ItemName": "Iron Sword",
      "Price": 100
    },
    "Item_Shield": {
      "ItemName": "Wooden Shield",
      "Price": 50
    }
  }
}
```

## Examples

```python
# Get full table info
manage_data_table(
    action="get_info",
    table_path="/Game/Data/Tables/DT_Items"
)

# Structure only (no row data)
manage_data_table(
    action="get_info",
    table_path="/Game/Data/Tables/DT_Items",
    include_rows=False
)

# Limit rows for large tables
manage_data_table(
    action="get_info",
    table_path="/Game/Data/Tables/DT_Items",
    max_rows=10
)
```

## Column Information

Each column entry provides:

| Field | Description |
|-------|-------------|
| `name` | Property name (column header) |
| `type` | Simplified type name |
| `cpp_type` | Full C++ type |
| `category` | Property category (if set in UPROPERTY) |
| `tooltip` | Property tooltip (if set in UPROPERTY) |
| `editable` | Whether the property can be modified |

## Notes

- The `table_path` can be with or without the asset suffix
- For large tables, use `max_rows` to limit response size
- Use `include_rows=False` when you only need structure info
- Row data is returned as a JSON object keyed by row name

## Related Actions

- `get_row_struct`: Get detailed column definitions
- `list_rows`: Get just the row names
- `get_row`: Get a single row's data
