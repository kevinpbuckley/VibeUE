# clear_instance_parameter_override

Remove a parameter override from an instance, reverting to the parent material's default value.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| instance_path | string | Yes | Full path to material instance |
| parameter_name | string | Yes | Name of parameter to clear |

## Examples

```json
{
  "action": "clear_instance_parameter_override",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant",
  "parameter_name": "Roughness"
}
```

## Returns

- `success`: true/false
- `parameter_name`: Parameter that was cleared
- `message`: Success/error message

## Tips

- After clearing, the instance will use the parent's default value
- If parent default changes, the instance will reflect that change
- Use `list_instance_parameters` to see which parameters are overridden
- Works for scalar, vector, and texture parameters
