# get_info

Get complete information about a data asset including all its property values.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | Yes | Path to the data asset |

## Returns

```json
{
  "success": true,
  "name": "DA_Sword",
  "path": "/Game/Data/Items/DA_Sword.DA_Sword",
  "class": "MyItemDataAsset",
  "class_path": "/Script/MyGame.MyItemDataAsset",
  "parent_classes": ["UPrimaryDataAsset", "UDataAsset"],
  "properties": {
    "ItemName": "Iron Sword",
    "Damage": 25,
    "Weight": 3.5,
    "bStackable": false
  }
}
```

## Examples

```python
# Get full asset information
manage_data_asset(action="get_info", asset_path="/Game/Data/Items/DA_Sword")
```

## Notes

- Returns all editable properties and their current values
- Use this to inspect what data is stored in an asset
- Property values are converted to JSON format
- Arrays and structs are fully serialized
