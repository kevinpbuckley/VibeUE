# set_properties

Set multiple material properties at once.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |
| properties | object | Yes | Dictionary of property_name: value pairs |

## Examples

Set multiple properties:
```json
{
  "action": "set_properties",
  "material_path": "/Game/Materials/MyMaterial",
  "properties": {
    "TwoSided": true,
    "BlendMode": "BLEND_Masked",
    "OpacityMaskClipValue": 0.33
  }
}
```

## Returns

- `success`: true/false
- `properties_set`: Number of properties successfully set
- `results`: Array of individual property results
- `message`: Summary message

## Tips

- More efficient than multiple `set_property` calls
- If one property fails, others may still succeed
- Check `results` array for individual success/failure
- Call `compile` after changing shader-affecting properties
