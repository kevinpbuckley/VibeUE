# find

Find actors in the level by name pattern, class, or tags.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| label_filter | string | No | Name/label pattern to filter by (supports wildcards) |
| class_filter | string | No | Filter by class name (e.g., "PointLight", "StaticMeshActor") |
| tag | string | No | Find actors with a specific tag |
| required_tags | array | No | Find actors with all of these tags |
| excluded_tags | array | No | Exclude actors with any of these tags |
| selected_only | bool | No | Only return currently selected actors |
| max_results | int | No | Limit number of results (alias: `limit`) |

Alternative parameter names: `filter`, `name_pattern` for label_filter; `class`, `filter_class` for class_filter.

## Examples

### Find by Name Pattern
```json
{
  "Action": "find",
  "ParamsJson": "{\"label_filter\": \"TestLight\"}"
}
```

### Find by Class
```json
{
  "Action": "find",
  "ParamsJson": "{\"class_filter\": \"PointLight\"}"
}
```

### Find by Tag
```json
{
  "Action": "find",
  "ParamsJson": "{\"tag\": \"Interactable\"}"
}
```

### Find with Multiple Tags
```json
{
  "Action": "find",
  "ParamsJson": "{\"required_tags\": [\"Enemy\", \"Spawned\"]}"
}
```

### Combine Filters with Limit
```json
{
  "Action": "find",
  "ParamsJson": "{\"class_filter\": \"StaticMeshActor\", \"max_results\": 20}"
}
```

### Find Selected Actors Only
```json
{
  "Action": "find",
  "ParamsJson": "{\"selected_only\": true}"
}
```

## Returns

```json
{
  "success": true,
  "actors": [
    {
      "actor_label": "TestLight1",
      "class_name": "PointLight"
    },
    {
      "actor_label": "TestLight2",
      "class_name": "PointLight"
    }
  ],
  "count": 2
}
```

## Tips

- Label filter does substring matching on actor labels
- Class filter matches the exact class name
- Multiple criteria are combined with AND logic
- Tags must be exact matches (case-sensitive)
- Use `max_results` to limit results for large levels
- More efficient than `list` when you know what you're looking for
