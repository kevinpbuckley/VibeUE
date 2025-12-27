# get_pins

Get all input and output pins for an expression node.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_id | string | Yes | ID of the expression |

## Examples

```json
{
  "action": "get_pins",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionMultiply_0"
}
```

## Returns

- `success`: true/false
- `expression_id`: Node identifier
- `inputs`: Array of input pin info (name, type, connected)
- `outputs`: Array of output pin info (name, type)

## Tips

- Essential for understanding how to connect nodes
- Input pin names are used as `target_input` in connect
- Output pin names are used as `source_output` in connect
- Common pins: "A", "B" for math nodes, "RGB", "R", "G", "B", "A" for texture samples
