# create

Create a new expression node in the material graph.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_class | string | Yes | Expression class name from discover_types |
| pos_x | int | No | X position in graph (default: 0) |
| pos_y | int | No | Y position in graph (default: 0) |

## Examples

Create a constant node:
```json
{
  "action": "create",
  "material_path": "/Game/Materials/M_Test",
  "expression_class": "MaterialExpressionConstant",
  "pos_x": -300,
  "pos_y": 0
}
```

Create a texture sample:
```json
{
  "action": "create",
  "material_path": "/Game/Materials/M_Test",
  "expression_class": "MaterialExpressionTextureSample",
  "pos_x": -400,
  "pos_y": 100
}
```

Create a multiply node:
```json
{
  "action": "create",
  "material_path": "/Game/Materials/M_Test",
  "expression_class": "MaterialExpressionMultiply"
}
```

## Returns

- `success`: true/false
- `expression_id`: ID of the created node (save for connecting!)
- `expression_class`: Class that was created
- `position`: Node position in graph

## Tips

- Use `discover_types` first to find the correct `expression_class`
- Save the returned `expression_id` - you need it for connect operations
- Common classes: MaterialExpressionConstant, MaterialExpressionConstant3Vector, MaterialExpressionTextureSample, MaterialExpressionMultiply, MaterialExpressionAdd
