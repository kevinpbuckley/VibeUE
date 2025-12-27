# get_property_info

Get detailed metadata about a property including valid values, current setting, and description.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |
| property_name | string | Yes | Name of property to inspect |

## Examples

```json
{
  "action": "get_property_info",
  "material_path": "/Game/Materials/MyMaterial",
  "property_name": "BlendMode"
}
```

## Returns

- `success`: true/false
- `property_name`: Name of property
- `current_value`: Current value
- `type`: Property type (bool, enum, float, etc.)
- `valid_values`: Array of valid values for enums
- `description`: Property description
- `category`: Property category

## Tips

- Especially useful for enum properties to see all valid options
- Use before `set_property` to understand what values are accepted
- Returns error if property doesn't exist
