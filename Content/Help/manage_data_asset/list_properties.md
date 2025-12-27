# list_properties

List available properties on a data asset or class.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | No* | Path to a data asset instance |
| `class_name` | string | No* | Name of a data asset class |
| `include_all` | bool | No | If true, show ALL properties including private/non-editable (default: false) |

*One of `asset_path` or `class_name` is required.

## Returns

```json
{
  "success": true,
  "class": "MyItemDataAsset",
  "properties": [
    {
      "name": "ItemName",
      "type": "FString",
      "category": "Item",
      "description": "Display name for the item",
      "read_only": false,
      "is_array": false,
      "defined_in": "MyItemDataAsset",
      "flags": "Edit, BlueprintVisible"
    }
  ],
  "count": 5,
  "include_all": false,
  "note": "Showing all properties including non-editable. Only properties with Edit/BlueprintVisible/SaveGame flags can be modified."
}
```

## Examples

```python
# List editable properties from an instance
manage_data_asset(
    action="list_properties",
    asset_path="/Game/Data/Items/DA_Sword"
)

# List properties from a class (without needing an instance)
manage_data_asset(
    action="list_properties",
    class_name="MyItemDataAsset"
)

# List ALL properties including private/internal ones (debugging)
manage_data_asset(
    action="list_properties",
    class_name="NetworkPhysicsSettingsDataAsset",
    include_all=True
)
```

## Notes

- By default, only properties marked with UPROPERTY(EditAnywhere), UPROPERTY(BlueprintReadWrite), or UPROPERTY(SaveGame) are shown
- Many engine DataAsset classes have internal data structures not exposed via UPROPERTY - these won't appear even with `include_all=true`
- Use `include_all=true` to diagnose why a class shows no properties
- Properties shown with `include_all` that don't have Edit flags cannot be modified through this tool

## Notes

- Use this before `set_property` to verify property names and types
- `defined_in` shows which class in the hierarchy defined the property
- `read_only` properties cannot be modified
- `is_array` indicates if the property is a TArray
