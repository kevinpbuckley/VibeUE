# add_row

Add a new row to a data table.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | Yes | Path to the data table asset |
| `row_name` | string | Yes | Unique name for the new row |
| `data` | object | No | Property values to set (JSON object) |

## Returns

```json
{
  "success": true,
  "message": "Added row 'Item_Potion' to /Game/Data/Tables/DT_Items",
  "table_path": "/Game/Data/Tables/DT_Items",
  "row_name": "Item_Potion"
}
```

## Examples

```python
# Add a row with data
manage_data_table(
    action="add_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_Potion",
    data={
        "ItemName": "Health Potion",
        "Description": "Restores 50 HP",
        "Price": 25,
        "HealAmount": 50
    }
)

# Add empty row (uses default values)
manage_data_table(
    action="add_row",
    table_path="/Game/Data/Tables/DT_Items",
    row_name="Item_NewItem"
)

# Add row with complex types
manage_data_table(
    action="add_row",
    table_path="/Game/Data/Tables/DT_Locations",
    row_name="Loc_Spawn",
    data={
        "LocationName": "Player Spawn",
        "Position": {"X": 100.0, "Y": 200.0, "Z": 0.0},
        "Rotation": {"Pitch": 0.0, "Yaw": 90.0, "Roll": 0.0}
    }
)
```

## Data Format

Property values should match the column types:

| Column Type | JSON Format | Example |
|-------------|-------------|---------|
| bool | boolean | `true` |
| int32 | number | `42` |
| float | number | `3.14` |
| FString | string | `"text"` |
| FVector | object | `{"X":0,"Y":0,"Z":0}` |
| FRotator | object | `{"Pitch":0,"Yaw":0,"Roll":0}` |
| FColor | object | `{"R":255,"G":0,"B":0,"A":255}` |
| Asset reference | string | `"/Game/Textures/Icon"` |
| Enum | string | `"EnumValueName"` |
| Array | array | `[1, 2, 3]` |

## Errors

| Error | Cause |
|-------|-------|
| `table_path is required` | Missing table_path parameter |
| `row_name is required` | Missing row_name parameter |
| `Row already exists` | A row with this name already exists |
| `Invalid property value` | Data doesn't match expected type |

## Notes

- Row names must be unique within the table
- If data is omitted, the row is created with default values
- Only provided properties are set; others use defaults
- The table is marked dirty after adding; use `manage_asset(action="save")` to persist

## Related Actions

- `get_row_struct`: Check column types before adding
- `update_row`: Modify an existing row
- `add_rows`: Add multiple rows at once
- `list_rows`: Verify the row was added
