# connect_to_output

Connect an expression to a material output property (BaseColor, Roughness, etc.).

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_id | string | Yes | ID of the expression to connect |
| material_property | string | Yes | Material output: BaseColor, Roughness, Metallic, Normal, etc. |
| output_name | string | No | Which output pin to use (default: first output) |

## Examples

Connect to BaseColor:
```json
{
  "action": "connect_to_output",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionMultiply_0",
  "material_property": "BaseColor"
}
```

Connect texture RGB to BaseColor:
```json
{
  "action": "connect_to_output",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionTextureSample_0",
  "material_property": "BaseColor",
  "output_name": "RGB"
}
```

Connect to Roughness:
```json
{
  "action": "connect_to_output",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionConstant_0",
  "material_property": "Roughness"
}
```

## Returns

- `success`: true/false
- `message`: Confirmation message
- `material_property`: Property that was connected

## Valid Material Properties

- `BaseColor` - Main color/albedo
- `Metallic` - 0 = non-metal, 1 = metal
- `Roughness` - 0 = smooth/shiny, 1 = rough
- `Normal` - Normal map input
- `EmissiveColor` - Self-illumination
- `Opacity` - For translucent materials
- `OpacityMask` - For masked materials
- `AmbientOcclusion` - AO channel
- `Specular` - Specular intensity

## Tips

- This connects to the MATERIAL OUTPUT node, not other expressions
- Use `get_output_properties` to see all available outputs
- Remember to compile the material after connecting
