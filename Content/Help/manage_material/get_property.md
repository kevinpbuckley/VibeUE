# get_property

Get the current value of a specific material property.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |
| property_name | string | Yes | Name of property (e.g., TwoSided, BlendMode) |

## Examples

```json
{
  "action": "get_property",
  "material_path": "/Game/Materials/MyMaterial",
  "property_name": "TwoSided"
}
```

```json
{
  "action": "get_property",
  "material_path": "/Game/Materials/MyMaterial",
  "property_name": "BlendMode"
}
```

## Returns

- `success`: true/false
- `property_name`: Name of property
- `value`: Current value

## Common Properties

- `TwoSided` - Boolean for two-sided rendering
- `BlendMode` - BLEND_Opaque, BLEND_Masked, BLEND_Translucent, BLEND_Additive, BLEND_Modulate
- `ShadingModel` - MSM_DefaultLit, MSM_Unlit, MSM_Subsurface, etc.
- `OpacityMaskClipValue` - Float threshold for masked materials

## Tips

- Property names are case-sensitive
- Use `list_properties` to see all available properties
- Use `get_property_info` for detailed metadata about valid values
