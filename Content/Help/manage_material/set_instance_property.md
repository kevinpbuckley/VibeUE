# set_instance_property

Set a property on a material instance.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| instance_path | string | Yes | Full path to material instance |
| property_name | string | Yes | Name of property to set |
| value | varies | Yes* | New value for the property |
| property_value | varies | Yes* | Alternative name for value parameter |

*Either `value` or `property_value` must be provided - both are accepted.

## Examples

```json
{
  "action": "set_instance_property",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant",
  "property_name": "TwoSided",
  "property_value": true
}
```

## Returns

- `success`: true/false
- `property_name`: Property that was set
- `value`: New value
- `message`: Success/error message

## Tips

- For parameter overrides, use the `set_instance_*_parameter` actions instead
- Both `value` and `property_value` parameter names work
- Not all properties can be changed on instances
