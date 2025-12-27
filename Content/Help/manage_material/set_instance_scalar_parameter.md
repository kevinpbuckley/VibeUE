# set_instance_scalar_parameter

Set or override a scalar (float) parameter on a material instance.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| instance_path | string | Yes | Full path to material instance |
| parameter_name | string | Yes | Name of scalar parameter |
| value | number | Yes | New value for the parameter |

## Examples

```json
{
  "action": "set_instance_scalar_parameter",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant",
  "parameter_name": "Roughness",
  "value": 0.8
}
```

```json
{
  "action": "set_instance_scalar_parameter",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant",
  "parameter_name": "Metallic",
  "value": 1.0
}
```

## Returns

- `success`: true/false
- `parameter_name`: Parameter that was set
- `value`: New value
- `message`: Success/error message

## Tips

- Parameter must exist in parent material
- Use `list_instance_parameters` to see available parameters
- Values are typically in 0.0-1.0 range but can vary
- Override persists until explicitly cleared
