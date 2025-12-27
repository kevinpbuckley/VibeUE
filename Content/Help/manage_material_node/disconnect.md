# disconnect

Disconnect an expression pin.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_id | string | Yes | ID of the expression |
| input_name | string | No | Specific input pin to disconnect (disconnects all if not specified) |

## Examples

Disconnect specific input:
```json
{
  "action": "disconnect",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionMultiply_0",
  "input_name": "A"
}
```

Disconnect all inputs:
```json
{
  "action": "disconnect",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionMultiply_0"
}
```

## Returns

- `success`: true/false
- `message`: Confirmation message
- `disconnected_pins`: List of disconnected pin names

## Tips

- Specify `input_name` to disconnect only one input
- Omit `input_name` to disconnect all inputs on the node
- Use `list_connections` first to see what's connected
