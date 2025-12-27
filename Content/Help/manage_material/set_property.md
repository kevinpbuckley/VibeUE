# set_property

Set a material property to a new value.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |
| property_name | string | Yes | Name of property to set |
| value | string/number/bool | Yes* | New value for the property |
| property_value | string/number/bool | Yes* | Alternative name for value parameter |

*Either `value` or `property_value` must be provided - both are accepted.

## Examples

Set boolean property:
```json
{
  "action": "set_property",
  "material_path": "/Game/Materials/MyMaterial",
  "property_name": "TwoSided",
  "property_value": true
}
```

Set enum property:
```json
{
  "action": "set_property",
  "material_path": "/Game/Materials/MyMaterial",
  "property_name": "BlendMode",
  "property_value": "BLEND_Masked"
}
```

Set float property:
```json
{
  "action": "set_property",
  "material_path": "/Game/Materials/MyMaterial",
  "property_name": "OpacityMaskClipValue",
  "value": 0.33
}
```

## Returns

- `success`: true/false
- `property_name`: Property that was set
- `value`: New value
- `message`: Success/error message

## Common Properties

| Property | Type | Valid Values |
|----------|------|--------------|
| TwoSided | bool | true, false |
| BlendMode | enum | BLEND_Opaque, BLEND_Masked, BLEND_Translucent, BLEND_Additive, BLEND_Modulate |
| ShadingModel | enum | MSM_DefaultLit, MSM_Unlit, MSM_Subsurface, MSM_ClearCoat, etc. |
| OpacityMaskClipValue | float | 0.0 - 1.0 |

## Tips

- Property names are case-sensitive
- Use `get_property_info` to see valid values for enum properties
- Call `compile` after changing shader-affecting properties
- Both `value` and `property_value` parameter names work
