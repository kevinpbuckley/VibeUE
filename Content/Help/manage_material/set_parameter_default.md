# set_parameter_default

Set the default value for a material parameter.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |
| parameter_name | string | Yes | Name of parameter to set |
| value | varies | Yes* | New default value |
| property_value | varies | Yes* | Alternative name for value parameter |

*Either `value` or `property_value` must be provided - both are accepted.

## Examples

Set scalar parameter:
```json
{
  "action": "set_parameter_default",
  "material_path": "/Game/Materials/MyMaterial",
  "parameter_name": "Roughness",
  "property_value": 0.75
}
```

Set vector parameter:
```json
{
  "action": "set_parameter_default",
  "material_path": "/Game/Materials/MyMaterial",
  "parameter_name": "BaseColor",
  "value": [1.0, 0.5, 0.0, 1.0]
}
```

## Returns

- `success`: true/false
- `parameter_name`: Parameter that was set
- `value`: New value
- `message`: Success/error message

## Tips

- This changes the default value in the parent material
- Material instances inherit this default unless they override it
- Call `compile` after changing parameter defaults
- Both `value` and `property_value` parameter names work
