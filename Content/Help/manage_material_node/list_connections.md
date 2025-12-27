# list_connections

List all connections in the material graph.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |

## Examples

```json
{
  "action": "list_connections",
  "material_path": "/Game/Materials/M_Test"
}
```

## Returns

- `success`: true/false
- `connections`: Array of connection objects
- `output_connections`: Material output connections
- `total_count`: Total number of connections

Each connection includes:
- `source_expression_id`: Source node
- `source_output`: Output pin name
- `target_expression_id`: Target node
- `target_input`: Input pin name

## Tips

- Shows both expression-to-expression and expression-to-output connections
- Useful for understanding material data flow
- Use before modifying to understand current wiring
