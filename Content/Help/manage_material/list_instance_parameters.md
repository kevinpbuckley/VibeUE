# list_instance_parameters

List all parameters on a material instance with their current and parent default values.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| instance_path | string | Yes | Full path to material instance |

## Examples

```json
{
  "action": "list_instance_parameters",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant"
}
```

## Returns

- `success`: true/false
- `scalar_parameters`: Array of scalar parameters with:
  - `name`: Parameter name
  - `value`: Current value (override or inherited)
  - `default_value`: Parent default value
  - `is_overridden`: Whether this instance overrides the value
- `vector_parameters`: Array of vector parameters (same structure)
- `texture_parameters`: Array of texture parameters (same structure)

## Tips

- Check `is_overridden` to see which values differ from parent
- Overridden parameters will persist even if parent changes
- Use `clear_instance_parameter_override` to revert to parent default
