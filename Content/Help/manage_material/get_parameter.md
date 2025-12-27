# get_parameter

Get information about a specific material parameter.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |
| parameter_name | string | Yes | Name of the parameter |

## Examples

```json
{
  "action": "get_parameter",
  "material_path": "/Game/Materials/MyMaterial",
  "parameter_name": "Roughness"
}
```

## Returns

- `success`: true/false
- `parameter_name`: Name of parameter
- `parameter_type`: Type (Scalar, Vector, Texture)
- `default_value`: Default value
- `group`: Parameter group
- `sort_priority`: Display sort order

## Tips

- Use `list_parameters` first to see available parameters
- Parameter names are case-sensitive
- Returns error if parameter doesn't exist
