# get_row

Get the data for a specific row in a data table.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | Yes | Path to the data table asset |
| `row_name` | string | Yes | Name of the row to retrieve |

## Returns

```json
{
  "success": true,
  "table_path": "/Game/Data/Tables/DT_Items",
  "row_name": "Item_Sword",
  "data": {
    "ItemName": "Iron Sword",
    "Description": "A basic sword forged from iron.",
    "Price": 100,
    "Damage": 25,
    "Icon": "/Game/Textures/Icons/T_Sword"
  }
}
```

## Examples

```python
# Get a single row
manage_data_table(
    action="get_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_Sword"
)

# Get weapon stats
manage_data_table(
    action="get_row",
    table_path="/Game/Data/Tables/DT_Weapons",
    row_name="Weapon_Fireball"
)
```

## Row Name Format

Row names in Unreal data tables:
- Are case-sensitive
- Must be unique within the table
- Typically use naming conventions like `Category_Name` or `ID_001`

## Error Handling

```json
{
  "success": false,
  "error": "Row 'InvalidRow' not found in table",
  "error_code": "ROW_NOT_FOUND"
}
```

## Notes

- Row names are case-sensitive
- Use `list_rows` first to get valid row names
- All properties of the row are returned as a JSON object
- Complex types (vectors, colors, etc.) are serialized to JSON objects

## Related Actions

- `list_rows`: Get all row names to find valid values
- `update_row`: Modify the retrieved row
- `get_info`: Get all rows at once
