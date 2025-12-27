# create_parameter

Create a new material parameter expression.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| parameter_type | string | Yes | Type: Scalar, Vector, Texture |
| parameter_name | string | Yes | Name for the parameter |
| default_value | varies | No | Default value for the parameter |
| group_name | string | No | Parameter group for organization |
| pos_x | int | No | X position in graph |
| pos_y | int | No | Y position in graph |

## Examples

Create scalar parameter:
```json
{
  "action": "create_parameter",
  "material_path": "/Game/Materials/M_Test",
  "parameter_type": "Scalar",
  "parameter_name": "Roughness",
  "default_value": 0.5
}
```

Create vector parameter:
```json
{
  "action": "create_parameter",
  "material_path": "/Game/Materials/M_Test",
  "parameter_type": "Vector",
  "parameter_name": "BaseColor",
  "default_value": "(R=1.0,G=0.5,B=0.0,A=1.0)",
  "group_name": "Color"
}
```

Create texture parameter:
```json
{
  "action": "create_parameter",
  "material_path": "/Game/Materials/M_Test",
  "parameter_type": "Texture",
  "parameter_name": "DiffuseTexture"
}
```

## Returns

- `success`: true/false
- `expression_id`: ID of created parameter node
- `parameter_name`: Name of the parameter
- `parameter_type`: Type that was created

## Tips

- Parameters are exposed in material instances
- Use group_name to organize parameters in the Material Instance editor
- Scalar = single float, Vector = RGBA color, Texture = texture reference
