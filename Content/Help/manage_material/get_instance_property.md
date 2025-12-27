# get_instance_property

Get a single property value from a material instance.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| instance_path | string | Yes | Full path to material instance |
| property_name | string | Yes | Name of property |

## Examples

```json
{
  "action": "get_instance_property",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant",
  "property_name": "TwoSided"
}
```

## Returns

- `success`: true/false
- `property_name`: Name of property
- `value`: Current value

## Tips

- Property names are case-sensitive
- Use `list_instance_properties` to see available properties
- For parameter values, use `list_instance_parameters` instead
