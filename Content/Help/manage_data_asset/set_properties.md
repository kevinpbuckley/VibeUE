# set_properties

Set multiple property values on a data asset in a single operation.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | Yes | Path to the data asset |
| `properties` | object | Yes | Object with property names as keys and new values |

## Returns

```json
{
  "success": true,
  "message": "Set 3 properties",
  "success": ["ItemName", "Damage", "Weight"],
  "failed": []
}
```

## Examples

```python
# Set multiple properties at once
manage_data_asset(
    action="set_properties",
    asset_path="/Game/Data/Items/DA_Sword",
    properties={
        "ItemName": "Legendary Fire Sword",
        "Damage": 75,
        "Weight": 4.5,
        "bStackable": false,
        "ItemRarity": "Legendary"
    }
)
```

## Notes

- More efficient than multiple `set_property` calls
- Partial success is possible - check `failed` array
- Invalid property names are reported in `failed`
- Changes are not saved automatically - call `save` afterwards
