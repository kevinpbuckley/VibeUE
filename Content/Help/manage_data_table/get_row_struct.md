# get_row_struct

Get detailed column definitions for a row struct type.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `table_path` | string | One required | Path to a data table (gets its row struct) |
| `struct_name` | string | One required | Row struct name directly |

**Note:** Provide either `table_path` OR `struct_name`, not both.

## Returns

```json
{
  "success": true,
  "struct_name": "FItemTableRow",
  "struct_path": "/Script/MyGame.ItemTableRow",
  "columns": [
    {
      "name": "ItemName",
      "type": "String",
      "cpp_type": "FString",
      "category": "",
      "tooltip": "Display name of the item",
      "editable": true
    },
    {
      "name": "Description",
      "type": "Text",
      "cpp_type": "FText",
      "category": "",
      "tooltip": "",
      "editable": true
    },
    {
      "name": "Price",
      "type": "Int32",
      "cpp_type": "int32",
      "category": "Economy",
      "tooltip": "",
      "editable": true
    },
    {
      "name": "Icon",
      "type": "SoftObjectPath",
      "cpp_type": "TSoftObjectPtr<UTexture2D>",
      "category": "Visual",
      "tooltip": "",
      "editable": true
    }
  ]
}
```

## Examples

```python
# Get struct from existing table
manage_data_table(
    action="get_row_struct",
    table_path="/Game/Data/Tables/DT_Items"
)

# Get struct by name (before creating a table)
manage_data_table(
    action="get_row_struct",
    struct_name="FItemTableRow"
)

# Check structure of weapon table
manage_data_table(
    action="get_row_struct",
    struct_name="FWeaponTableRow"
)
```

## Use Cases

1. **Before Creating Tables**: Check what columns a row struct provides
2. **Data Entry Planning**: Understand required fields before adding rows
3. **Type Verification**: Confirm property types for JSON data formatting
4. **Documentation**: Generate documentation for data table schemas

## Column Type Mapping

Common type mappings:

| cpp_type | JSON Type | Example |
|----------|-----------|---------|
| `bool` | boolean | `true` |
| `int32`, `int64` | number | `42` |
| `float`, `double` | number | `3.14` |
| `FString` | string | `"Hello"` |
| `FName` | string | `"ItemName"` |
| `FText` | string | `"Localized Text"` |
| `FVector` | object | `{"X":0,"Y":0,"Z":0}` |
| `FRotator` | object | `{"Pitch":0,"Yaw":0,"Roll":0}` |
| `FColor` | object | `{"R":255,"G":0,"B":0,"A":255}` |
| `FLinearColor` | object | `{"R":1,"G":0,"B":0,"A":1}` |
| `TSoftObjectPtr<T>` | string | `"/Game/Textures/Icon"` |
| `TArray<T>` | array | `[1, 2, 3]` |
| Enum | string | `"EnumValueName"` |

## Notes

- Inherited properties from `FTableRowBase` are filtered out
- Only editable properties are typically shown
- Use this to understand what data format is expected for `add_row` and `update_row`

## Related Actions

- `search_row_types`: Find available row struct types
- `create`: Create a table with the discovered struct
- `add_row`: Add rows knowing the column structure
