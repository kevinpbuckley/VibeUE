# update_row

Update an existing row's data in a data table.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | Yes | Path to the data table asset |
| `row_name` | string | Yes | Name of the row to update |
| `data` | object | Yes | Property values to update (JSON object) |

## Returns

```json
{
  "success": true,
  "message": "Updated row 'Item_Sword' in /Game/Data/Tables/DT_Items",
  "table_path": "/Game/Data/Tables/DT_Items",
  "row_name": "Item_Sword",
  "updated_properties": ["Price", "Damage"]
}
```

## Examples

```python
# Update specific properties
manage_data_table(
    action="update_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_Sword",
    data={
        "Price": 150,
        "Damage": 30
    }
)

# Update a single property
manage_data_table(
    action="update_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_Potion",
    data={
        "HealAmount": 75
    }
)

# Update complex properties
manage_data_table(
    action="update_row",
    table_path="/Game/Data/Tables/DT_Locations",
    row_name="Loc_Spawn",
    data={
        "Position": {"X": 200.0, "Y": 300.0, "Z": 50.0}
    }
)
```

## Partial Updates

Only properties included in `data` are modified. Other properties retain their existing values.

```python
# This only updates Price, leaving ItemName, Description, etc. unchanged
manage_data_table(
    action="update_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_Sword",
    data={"Price": 200}
)
```

## Errors

| Error | Cause |
|-------|-------|
| `table_path is required` | Missing table_path parameter |
| `row_name is required` | Missing row_name parameter |
| `data is required` | Missing data parameter |
| `Row not found` | Specified row doesn't exist |
| `Invalid property value` | Data doesn't match expected type |
| `Unknown property` | Property name not in row struct |

## Notes

- The row must already exist (use `add_row` for new rows)
- Only provided properties are updated
- Property names are case-sensitive
- The response includes list of actually modified properties
- Table is marked dirty; use `manage_asset(action="save")` to persist

## Related Actions

- `get_row`: Get current row data before updating
- `add_row`: Add a new row if it doesn't exist
- `get_row_struct`: Check valid property names
