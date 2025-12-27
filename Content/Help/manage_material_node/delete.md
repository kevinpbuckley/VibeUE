# delete

Delete an expression node from the material graph.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_id | string | Yes | ID of the expression to delete |

## Examples

```json
{
  "action": "delete",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionConstant_0"
}
```

## Returns

- `success`: true/false
- `message`: Confirmation message
- `deleted_expression_id`: ID that was deleted

## Tips

- Deleting a node removes all its connections automatically
- Use `list` action first to find expression IDs
- Consider disconnecting important connections before deletion
