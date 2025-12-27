# add_rows

Add multiple rows to a data table in a single operation.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | Yes | Path to the data table asset |
| `rows` | object | Yes | Object with row_name keys and data values |

## Returns

```json
{
  "success": true,
  "message": "Added 3 rows to /Game/Data/Tables/DT_Items",
  "added_rows": ["Item_Sword", "Item_Shield", "Item_Potion"],
  "failed_rows": {
    "Item_Existing": "Row already exists"
  }
}
```

## Examples

```python
# Add multiple rows
manage_data_table(
    action="add_rows",
    table_path="/Game/Data/Tables/DT_Items",
    rows={
        "Item_Sword": {
            "ItemName": "Iron Sword",
            "Price": 100,
            "Damage": 25
        },
        "Item_Shield": {
            "ItemName": "Wooden Shield",
            "Price": 50,
            "Defense": 10
        },
        "Item_Potion": {
            "ItemName": "Health Potion",
            "Price": 25,
            "HealAmount": 50
        }
    }
)

# Add location data
manage_data_table(
    action="add_rows",
    table_path="/Game/Data/Tables/DT_Locations",
    rows={
        "Loc_Spawn": {
            "Name": "Player Spawn",
            "Position": {"X": 0, "Y": 0, "Z": 100}
        },
        "Loc_Shop": {
            "Name": "Item Shop",
            "Position": {"X": 500, "Y": 200, "Z": 0}
        }
    }
)
```

## Data Format

The `rows` parameter is a JSON object where:
- Each key is the row name
- Each value is an object containing the row's property values

```python
rows = {
    "RowName1": {"Property1": value1, "Property2": value2},
    "RowName2": {"Property1": value1, "Property2": value2}
}
```

## Partial Success

The operation continues even if some rows fail:

```json
{
  "success": true,
  "message": "Added 2 rows to /Game/Data/Tables/DT_Items",
  "added_rows": ["Item_New1", "Item_New2"],
  "failed_rows": {
    "Item_Existing": "Row already exists",
    "Item_BadData": "Invalid property: UnknownProp"
  }
}
```

## Errors

| Error | Cause |
|-------|-------|
| `table_path is required` | Missing table_path parameter |
| `rows is required` | Missing or invalid rows parameter |
| Individual row errors | Reported in `failed_rows` object |

## Notes

- This is more efficient than calling `add_row` multiple times
- Rows that already exist will fail (won't overwrite)
- Use `import_json` with `mode="replace"` to overwrite existing rows
- The table is marked dirty; use `manage_asset(action="save")` to persist

## Related Actions

- `add_row`: Add a single row
- `import_json`: Import with replace option
- `clear_rows`: Clear table before bulk add
