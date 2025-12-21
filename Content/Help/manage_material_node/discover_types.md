# discover_types

Discover available material expression types for creating nodes.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| search_term | string | No | Filter by keyword (e.g., "Constant", "Texture", "Add") |
| category | string | No | Filter by category (prefer search_term) |
| max_results | int | No | Maximum results to return (default: 100) |

## Examples

Find constant expressions:
```json
{
  "action": "discover_types",
  "search_term": "Constant"
}
```

Find texture-related expressions:
```json
{
  "action": "discover_types",
  "search_term": "Texture"
}
```

Find math operations:
```json
{
  "action": "discover_types",
  "search_term": "Add"
}
```

## Returns

- `success`: true/false
- `expression_types`: Array of available expression types with class names
- `total_count`: Number of matching types

## Tips

- Use `search_term` instead of `category` for better results
- Common searches: "Constant", "Texture", "Parameter", "Add", "Multiply", "Lerp"
- The returned `class_name` is used as `expression_class` in create action
