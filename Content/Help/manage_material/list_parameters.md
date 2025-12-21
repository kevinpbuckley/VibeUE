# list_parameters

List all material parameters (scalar, vector, texture) with their default values.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |

## Examples

```json
{
  "action": "list_parameters",
  "material_path": "/Game/Materials/MyMaterial"
}
```

## Returns

- `success`: true/false
- `scalar_parameters`: Array of scalar (float) parameters
- `vector_parameters`: Array of vector/color parameters
- `texture_parameters`: Array of texture parameters

Each parameter includes:
- `name`: Parameter name
- `default_value`: Default value
- `group`: Parameter group (if set)

## Tips

- Parameters are exposed values that can be overridden in material instances
- Use `get_parameter` for details about a specific parameter
- Material instances can override these defaults
- Parameters are created by adding Parameter nodes in the material graph
