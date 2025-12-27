# list

List all expression nodes in the material graph.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |

## Examples

```json
{
  "action": "list",
  "material_path": "/Game/Materials/M_Test"
}
```

## Returns

- `success`: true/false
- `expressions`: Array of expression info objects
- `total_count`: Number of expressions

Each expression includes:
- `expression_id`: Unique identifier
- `class_name`: Expression type
- `position`: Graph position
- `description`: Human-readable description

## Tips

- Use this to find expression IDs for other operations
- Helpful for understanding material structure
- Returns all nodes including parameters and comments
