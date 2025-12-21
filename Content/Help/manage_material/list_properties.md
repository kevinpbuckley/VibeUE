# list_properties

List all editable properties of a material with their current values.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |
| include_advanced | boolean | No | Include advanced properties (default: false) |

## Examples

List basic properties:
```json
{
  "action": "list_properties",
  "material_path": "/Game/Materials/MyMaterial"
}
```

Include advanced properties:
```json
{
  "action": "list_properties",
  "material_path": "/Game/Materials/MyMaterial",
  "include_advanced": true
}
```

## Returns

- `success`: true/false
- `properties`: Array of property objects with:
  - `name`: Property name
  - `value`: Current value
  - `type`: Property type
  - `category`: Property category

## Tips

- Advanced properties include less commonly used settings
- Use `get_property_info` for details about a specific property
- Property names are case-sensitive when using `set_property`
