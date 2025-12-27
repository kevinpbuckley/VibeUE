# list_instance_properties

List all editable properties on a material instance.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| instance_path | string | Yes | Full path to material instance |
| include_advanced | boolean | No | Include advanced properties |

## Examples

```json
{
  "action": "list_instance_properties",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant"
}
```

## Returns

- `success`: true/false
- `properties`: Array of property objects with name, value, type

## Tips

- Material instances have fewer editable properties than base materials
- Most visual changes come from parameter overrides, not properties
- Use `list_instance_parameters` for parameter values
