# set_property

Set a single property value on a data asset.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `asset_path` | string | Yes | Path to the data asset |
| `property_name` | string | Yes | Name of the property to set |
| `property_value` | any | Yes | New value for the property |

## Returns

```json
{
  "success": true,
  "message": "Set property ItemName",
  "property_name": "ItemName",
  "new_value": "Fire Sword"
}
```

## Value Formats by Type

| Property Type | JSON Value Format |
|--------------|-------------------|
| `int32`, `float` | Number: `25`, `3.14` |
| `bool` | Boolean: `true`, `false` |
| `FString`, `FName` | String: `"MyValue"` |
| `Enum` | String (name): `"TypeA"` or Number: `0` |
| `TArray<>` | Array: `[1, 2, 3]` |
| `UObject*` | String (path): `"/Game/Asset.Asset"` |
| `Struct` | Object: `{"X": 1.0, "Y": 2.0}` |

## Examples

```python
# Set string property
manage_data_asset(
    action="set_property",
    asset_path="/Game/Data/Items/DA_Sword",
    property_name="ItemName",
    property_value="Fire Sword"
)

# Set numeric property
manage_data_asset(
    action="set_property",
    asset_path="/Game/Data/Items/DA_Sword",
    property_name="Damage",
    property_value=50
)

# Set enum by name
manage_data_asset(
    action="set_property",
    asset_path="/Game/Data/Items/DA_Sword",
    property_name="ItemRarity",
    property_value="Legendary"
)

# Set array property
manage_data_asset(
    action="set_property",
    asset_path="/Game/Data/Items/DA_Sword",
    property_name="StatusEffects",
    property_value=["Burn", "Bleed"]
)
```

## Notes

- Changes are not saved automatically - call `save` afterwards
- Use `list_properties` to verify property names and types
- For multiple properties, prefer `set_properties` for efficiency
