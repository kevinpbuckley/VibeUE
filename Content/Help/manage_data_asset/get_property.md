# get_property

Get a single property value from a data asset.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | Yes | Path to the data asset |
| `property_name` | string | Yes | Name of the property to get |

## Returns

```json
{
  "success": true,
  "property_name": "Damage",
  "type": "int32",
  "value": 50
}
```

## Examples

```python
# Get a property value
manage_data_asset(
    action="get_property",
    asset_path="/Game/Data/Items/DA_Sword",
    property_name="Damage"
)

# Get an array property
manage_data_asset(
    action="get_property",
    asset_path="/Game/Data/Items/DA_Sword",
    property_name="StatusEffects"
)
```

## Notes

- For all properties at once, use `get_info` instead
- Returns the value in JSON-compatible format
- Complex types (arrays, structs) are fully serialized
