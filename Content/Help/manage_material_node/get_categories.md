# get_categories

Get all available expression type categories.

## Parameters

No parameters required.

## Examples

```json
{
  "action": "get_categories"
}
```

## Returns

- `success`: true/false
- `categories`: Array of category names
- `total_count`: Number of categories

## Tips

- Categories include: Constants, Math, Texture, Parameters, Utility, etc.
- Use with discover_types to filter by category
- Prefer using search_term in discover_types for more reliable filtering
