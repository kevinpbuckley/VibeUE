# connect

Connect two expression nodes together.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| source_expression_id | string | Yes | ID of the source (output) node |
| source_output | string | No | Output pin name (default: first output) |
| target_expression_id | string | Yes | ID of the target (input) node |
| target_input | string | Yes | Input pin name to connect to |

## Examples

Connect constant to multiply A input:
```json
{
  "action": "connect",
  "material_path": "/Game/Materials/M_Test",
  "source_expression_id": "MaterialExpressionConstant_0",
  "target_expression_id": "MaterialExpressionMultiply_0",
  "target_input": "A"
}
```

Connect texture RGB to multiply B input:
```json
{
  "action": "connect",
  "material_path": "/Game/Materials/M_Test",
  "source_expression_id": "MaterialExpressionTextureSample_0",
  "source_output": "RGB",
  "target_expression_id": "MaterialExpressionMultiply_0",
  "target_input": "B"
}
```

## Returns

- `success`: true/false
- `message`: Confirmation message
- `connection`: Connection details

## Tips

- Use `get_pins` to find valid pin names
- source_output defaults to the first output if not specified
- Common input names: "A", "B", "Alpha", "Base", "Blend"
- Common output names: "RGB", "R", "G", "B", "A" (for textures)
