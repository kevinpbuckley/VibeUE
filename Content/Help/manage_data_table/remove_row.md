# remove_row

Remove a row from a data table.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | Yes | Path to the data table asset |
| `row_name` | string | Yes | Name of the row to remove |

## Returns

```json
{
  "success": true,
  "message": "Removed row 'Item_OldItem' from /Game/Data/Tables/DT_Items"
}
```

## Examples

```python
# Remove a single row
manage_data_table(
    action="remove_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_OldItem"
)

# Alternative action name
manage_data_table(
    action="delete_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_Deprecated"
)
```

## Errors

| Error | Cause |
|-------|-------|
| `table_path is required` | Missing table_path parameter |
| `row_name is required` | Missing row_name parameter |
| `Row not found` | Specified row doesn't exist |

## Notes

- This action is destructive and cannot be undone
- Use `delete_row` as an alias for `remove_row`
- The table is marked dirty after removal; use `manage_asset(action="save")` to persist
- To remove all rows, use `clear_rows` instead

## Workflow

```python
# 1. Verify row exists
manage_data_table(
    action="get_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_ToDelete"
)

# 2. Remove the row
manage_data_table(
    action="remove_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_ToDelete"
)

# 3. Save the table
manage_asset(
    action="save",
    asset_path="/Game/Data/Tables/DT_Items"
)
```

## Related Actions

- `list_rows`: Get valid row names
- `clear_rows`: Remove all rows at once
- `add_row`: Add rows back if needed
