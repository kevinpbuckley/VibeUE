# set_instance_vector_parameter

Set or override a vector/color parameter on a material instance.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| instance_path | string | Yes | Full path to material instance |
| parameter_name | string | Yes | Name of vector parameter |
| value | array or string | Yes | Color as `[R,G,B,A]` array or Unreal color string |

## Examples

Using array format (recommended):
```json
{
  "action": "set_instance_vector_parameter",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant",
  "parameter_name": "BaseColor",
  "value": [1.0, 0.0, 0.0, 1.0]
}
```

Using Unreal color string:
```json
{
  "action": "set_instance_vector_parameter",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant",
  "parameter_name": "BaseColor",
  "value": "(R=1.0,G=0.0,B=0.0,A=1.0)"
}
```

## Returns

- `success`: true/false
- `parameter_name`: Parameter that was set
- `value`: New value
- `message`: Success/error message

## Color Values

- Values are LINEAR, not sRGB (0.0-1.0 range)
- Array format: `[Red, Green, Blue, Alpha]`
- Alpha defaults to 1.0 if only 3 values provided
- For sRGB colors, convert: `linear = srgb^2.2`

## Tips

- Parameter must exist in parent material
- Use for BaseColor, EmissiveColor, and other color parameters
- Colors in UE are typically linear space
