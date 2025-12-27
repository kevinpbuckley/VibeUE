# move

Move an expression node to a new position in the graph.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_id | string | Yes | ID of the expression to move |
| pos_x | int | Yes | New X position |
| pos_y | int | Yes | New Y position |

## Examples

```json
{
  "action": "move",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionConstant_0",
  "pos_x": -200,
  "pos_y": 100
}
```

## Returns

- `success`: true/false
- `expression_id`: ID of moved node
- `new_position`: Updated position

## Tips

- Negative X positions place nodes to the left (input side)
- Organize nodes for readability - inputs on left, outputs on right
- Moving doesn't affect connections
