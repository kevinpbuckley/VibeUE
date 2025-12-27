# list

List data tables in the project.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `row_struct` | string | No | Filter by row struct type |
| `path` | string | No | Asset path to search (default: `/Game`) |

## Returns

```json
{
  "success": true,
  "tables": [
    {
      "name": "DT_Items",
      "path": "/Game/Data/Tables/DT_Items.DT_Items",
      "row_struct": "FItemTableRow",
      "row_struct_path": "/Script/MyGame.ItemTableRow"
    },
    {
      "name": "DT_Weapons",
      "path": "/Game/Data/Tables/DT_Weapons.DT_Weapons",
      "row_struct": "FWeaponTableRow",
      "row_struct_path": "/Script/MyGame.WeaponTableRow"
    }
  ],
  "count": 2
}
```

## Examples

```python
# List all data tables
manage_data_table(action="list")

# Filter by row struct
manage_data_table(
    action="list",
    row_struct="FItemTableRow"
)

# Search in specific folder
manage_data_table(
    action="list",
    path="/Game/Data/Items"
)

# Combined filter
manage_data_table(
    action="list",
    row_struct="FItemTableRow",
    path="/Game/Data"
)
```

## Notes

- Default search path is `/Game`
- Row struct filter matches by struct name
- Results include both the table name and its row struct type
- For detailed table info, use `get_info` with the returned path

## Related Actions

- `get_info`: Get detailed information about a specific table
- `create`: Create a new data table
