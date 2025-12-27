# get_instance_info

Get comprehensive information about a material instance.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| instance_path | string | Yes | Full path to material instance |

## Examples

```json
{
  "action": "get_instance_info",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant"
}
```

## Returns

- `success`: true/false
- `instance_path`: Path to instance
- `instance_name`: Name of instance
- `parent_material`: Path to parent material
- `scalar_parameter_overrides`: List of overridden scalar parameters
- `vector_parameter_overrides`: List of overridden vector parameters
- `texture_parameter_overrides`: List of overridden texture parameters
- `properties`: Instance-specific properties

## Tips

- Use this to understand what's overridden vs inherited
- Check `parent_material` to find the source material
- Override counts show how many parameters differ from parent
