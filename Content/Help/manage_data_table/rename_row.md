# rename_row

Rename a row in a data table.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | Yes | Path to the data table asset |
| `row_name` | string | Yes | Current name of the row |
| `new_name` | string | Yes | New name for the row |

## Returns

```json
{
  "success": true,
  "message": "Renamed row 'Item_OldName' to 'Item_NewName' in /Game/Data/Tables/DT_Items"
}
```

## Examples

```python
# Rename a row
manage_data_table(
    action="rename_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_OldName",
    new_name="Item_NewName"
)

# Fix naming convention
manage_data_table(
    action="rename_row",
    table_path="/Game/Data/Tables/DT_Weapons",
    row_name="sword",
    new_name="Weapon_Sword"
)
```

## Errors

| Error | Cause |
|-------|-------|
| `table_path is required` | Missing table_path parameter |
| `row_name is required` | Missing row_name parameter |
| `new_name is required` | Missing new_name parameter |
| `Row not found` | Original row doesn't exist |
| `Row already exists` | New name already in use |

## Notes

- Row names must be unique within the table
- The row's data is preserved during rename
- Row names are case-sensitive
- The table is marked dirty; use `manage_asset(action="save")` to persist
- Blueprint references to the old row name may need updating

## Workflow

```python
# 1. List current rows
manage_data_table(
    action="list_rows",
    table_path="/Game/Data/Tables/DT_Items"
)

# 2. Rename as needed
manage_data_table(
    action="rename_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="OldRowName",
    new_name="NewRowName"
)

# 3. Verify the change
manage_data_table(
    action="list_rows",
    table_path="/Game/Data/Tables/DT_Items"
)
```

## Related Actions

- `list_rows`: See current row names
- `get_row`: Get row data before/after rename
- `update_row`: Update row data (not name)
