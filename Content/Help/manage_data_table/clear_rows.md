# clear_rows

Remove all rows from a data table.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | Yes | Path to the data table asset |
| `confirm` | bool | Yes | Must be `true` to confirm destructive operation |

## Returns

```json
{
  "success": true,
  "message": "Cleared 15 rows from /Game/Data/Tables/DT_Items",
  "cleared_count": 15
}
```

## Examples

```python
# Clear all rows (requires confirmation)
manage_data_table(
    action="clear_rows",
    table_path="/Game/Data/Tables/DT_Items",
    confirm=True
)
```

## Safety Check

The `confirm=True` parameter is required to prevent accidental data loss:

```python
# This will fail
manage_data_table(
    action="clear_rows",
    table_path="/Game/Data/Tables/DT_Items"
)
# Error: "confirm=true is required for clear_rows (destructive operation)"

# This will succeed
manage_data_table(
    action="clear_rows",
    table_path="/Game/Data/Tables/DT_Items",
    confirm=True
)
```

## Errors

| Error | Cause |
|-------|-------|
| `table_path is required` | Missing table_path parameter |
| `confirm=true is required` | Safety check not passed |
| `Table not found` | Invalid table path |

## Use Cases

1. **Reset Table**: Clear before reimporting data
2. **Development**: Start fresh with test data
3. **Cleanup**: Remove all entries before deletion

## Workflow

```python
# 1. Export current data (optional backup)
backup = manage_data_table(
    action="export_json",
    table_path="/Game/Data/Tables/DT_Items"
)

# 2. Clear the table
manage_data_table(
    action="clear_rows",
    table_path="/Game/Data/Tables/DT_Items",
    confirm=True
)

# 3. Add new data
manage_data_table(
    action="add_rows",
    table_path="/Game/Data/Tables/DT_Items",
    rows={...}
)

# 4. Save changes
manage_asset(
    action="save",
    asset_path="/Game/Data/Tables/DT_Items"
)
```

## Notes

- This operation is destructive and cannot be undone
- Export data first if you need a backup
- The table structure (columns) is preserved, only rows are removed
- The table is marked dirty; use `manage_asset(action="save")` to persist

## Related Actions

- `export_json`: Backup data before clearing
- `import_json`: Restore data after clearing
- `remove_row`: Remove specific rows instead
