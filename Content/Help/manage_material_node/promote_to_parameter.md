# promote_to_parameter

Promote an expression to a material parameter.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_id | string | Yes | ID of the expression to promote |
| parameter_name | string | No | Name for the new parameter |

## Examples

Promote constant to parameter:
```json
{
  "action": "promote_to_parameter",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionConstant_0",
  "parameter_name": "Roughness_Value"
}
```

## Returns

- `success`: true/false
- `old_expression_id`: Original expression ID
- `new_expression_id`: New parameter expression ID
- `parameter_name`: Name of created parameter
- `message`: Confirmation message

## Tips

- Converts constant expressions to parameter expressions
- Parameters can be modified in material instances
- Original connections are preserved
- Common use: Convert Constant to ScalarParameter, Constant3Vector to VectorParameter
