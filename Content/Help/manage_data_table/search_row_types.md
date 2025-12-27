# search_row_types

Find available row struct types that can be used to create data tables.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `search_filter` | string | No | Filter results by name (case-insensitive partial match) |
| `search_text` | string | No | Alias for search_filter |

## Returns

```json
{
  "success": true,
  "types": [
    {
      "name": "FItemTableRow",
      "path": "/Script/MyGame.ItemTableRow",
      "module": "MyGame",
      "parent_struct": "FTableRowBase",
      "is_native": true,
      "properties": ["ItemName", "Description", "Price", "Damage"]
    }
  ],
  "count": 1,
  "filter": "Item"
}
```

## Examples

```python
# List all available row struct types
manage_data_table(action="search_row_types")

# Filter by name
manage_data_table(
    action="search_row_types",
    search_filter="Item"
)

# Search for weapon-related structs
manage_data_table(
    action="search_row_types",
    search_text="Weapon"
)
```

## Notes

- Only returns structs that inherit from `FTableRowBase`
- Both native (C++) and Blueprint row structs are returned
- Use the `name` value when calling `create` action
- The `properties` array shows column names without the full type info
- For detailed column info, use `get_row_struct` after finding your type

## Related Actions

- `create`: Create a data table using a discovered row struct
- `get_row_struct`: Get detailed column definitions for a row struct
