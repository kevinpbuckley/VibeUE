# list_rows

List all row names in a data table.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | Yes | Path to the data table asset |

## Returns

```json
{
  "success": true,
  "table_path": "/Game/Data/Tables/DT_Items",
  "rows": [
    "Item_Sword",
    "Item_Shield",
    "Item_Potion",
    "Item_Armor"
  ],
  "count": 4
}
```

## Examples

```python
# List all rows in a table
manage_data_table(
    action="list_rows",
    table_path="/Game/Data/Tables/DT_Items"
)

# Check if table has data
result = manage_data_table(
    action="list_rows",
    table_path="/Game/Data/Tables/DT_Weapons"
)
# Check result["count"] to see if empty
```

## Use Cases

1. **Overview**: Get a quick list of all entries in a table
2. **Validation**: Check if expected rows exist
3. **Iteration**: Get row names for subsequent `get_row` calls
4. **Count**: Check how many rows are in the table

## Notes

- Returns only the row names, not the row data
- For row data, use `get_row` with a specific row name
- For all data at once, use `get_info` with `include_rows=True`
- Row names are returned in the order they appear in the table

## Related Actions

- `get_row`: Get data for a specific row
- `get_info`: Get full table info including rows
- `add_row`: Add a new row
- `remove_row`: Remove a row by name
