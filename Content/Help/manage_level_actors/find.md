# find

Find actors in the level by name pattern or class.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| NamePattern | string | No | Name pattern with wildcards (e.g., "*Light*", "BP_*") |
| Class | string | No | Filter by exact class name |
| Tag | string | No | Find actors with a specific tag |

## Examples

### Find by Name Pattern
```json
{
  "Action": "find",
  "ParamsJson": "{\"NamePattern\": \"*Enemy*\"}"
}
```

### Find by Class
```json
{
  "Action": "find",
  "ParamsJson": "{\"Class\": \"PointLight\"}"
}
```

### Find by Tag
```json
{
  "Action": "find",
  "ParamsJson": "{\"Tag\": \"Interactable\"}"
}
```

### Combine Filters
```json
{
  "Action": "find",
  "ParamsJson": "{\"NamePattern\": \"*Door*\", \"Tag\": \"Locked\"}"
}
```

## Returns

```json
{
  "Success": true,
  "Actors": [
    {
      "Name": "BP_Enemy_0",
      "Class": "BP_Enemy_C",
      "Location": {"X": 500, "Y": 0, "Z": 100}
    },
    {
      "Name": "BP_Enemy_1",
      "Class": "BP_Enemy_C",
      "Location": {"X": 800, "Y": 200, "Z": 100}
    }
  ],
  "Count": 2
}
```

## Tips

- Use `*` wildcard to match any characters in name
- Multiple criteria are combined with AND logic
- Tags must be exact matches
- More efficient than listing all actors when searching
