# get_class_info

Get information about a data asset class including its properties.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `class_name` | string | Yes | Name of the data asset class |

## Returns

```json
{
  "success": true,
  "name": "MyItemDataAsset",
  "path": "/Script/MyGame.MyItemDataAsset",
  "is_abstract": false,
  "is_native": true,
  "parent_classes": ["UPrimaryDataAsset", "UDataAsset"],
  "properties": [
    {
      "name": "ItemName",
      "type": "FString",
      "defined_in": "MyItemDataAsset"
    }
  ]
}
```

## Examples

```python
# Get class information
manage_data_asset(action="get_class_info", class_name="MyItemDataAsset")
```

## Notes

- Use this to understand a class before creating instances
- Shows all inherited properties from parent classes
- `is_native` indicates C++ vs Blueprint-defined class
- `is_abstract` classes cannot be instantiated
