# create

Create a new data table asset with a specified row struct type.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `row_struct` | string | Yes | Row struct type name (use `search_row_types` to find) |
| `struct_name` | string | No | Alias for row_struct |
| `asset_path` | string | No | Destination folder path (default: `/Game/Data`) |
| `asset_name` | string | Yes* | Name for the new table (*can be included in asset_path) |

## Returns

```json
{
  "success": true,
  "message": "Created data table: /Game/Data/Tables/DT_Items",
  "asset_path": "/Game/Data/Tables/DT_Items.DT_Items",
  "asset_name": "DT_Items",
  "row_struct": "FItemTableRow"
}
```

## Examples

```python
# Basic creation
manage_data_table(
    action="create",
    row_struct="FItemTableRow",
    asset_path="/Game/Data/Tables",
    asset_name="DT_Items"
)

# With path including name
manage_data_table(
    action="create",
    row_struct="FItemTableRow",
    asset_path="/Game/Data/Tables/DT_Items"
)

# Using struct_name alias
manage_data_table(
    action="create",
    struct_name="FWeaponTableRow",
    asset_path="/Game/Data",
    asset_name="DT_Weapons"
)
```

## Workflow

1. Use `search_row_types` to find available row struct types
2. Create the data table with the chosen struct
3. Use `add_row` or `add_rows` to populate with data

```python
# Step 1: Find row struct types
manage_data_table(action="search_row_types", search_filter="Item")
# Returns: FItemTableRow

# Step 2: Create the table
manage_data_table(
    action="create",
    row_struct="FItemTableRow",
    asset_path="/Game/Data/Items",
    asset_name="DT_Items"
)

# Step 3: Add data
manage_data_table(
    action="add_row",
    table_path="/Game/Data/Items/DT_Items",
    row_name="Sword_001",
    data={"ItemName": "Iron Sword", "Price": 100}
)
```

## Notes

- The row struct must exist and inherit from `FTableRowBase`
- Asset will be created in memory; use `manage_asset(action="save")` to persist
- The asset is automatically marked dirty after creation
- Use naming convention DT_ prefix for data tables

## Errors

| Error | Cause |
|-------|-------|
| `row_struct is required` | Missing row_struct parameter |
| `asset_name is required` | Missing asset_name parameter |
| `Row struct not found` | Specified struct doesn't exist |
| `Asset already exists` | Table with same name already exists at path |

## Related Actions

- `search_row_types`: Find available row struct types
- `add_row`: Add rows to the created table
- `get_info`: Verify the created table
