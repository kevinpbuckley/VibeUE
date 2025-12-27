# search

Search for assets in the Unreal Engine project by name, path pattern, or class type.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| SearchPattern | string | Yes | Search pattern with wildcards (e.g., "*Character*", "BP_*") |
| ClassFilter | string | No | Filter by asset class type (e.g., "Blueprint", "Texture2D", "StaticMesh") |
| Path | string | No | Limit search to a specific content path |
| MaxResults | number | No | Maximum number of results to return (default: 100) |

## Examples

### Search by Name Pattern
```json
{
  "Action": "search",
  "ParamsJson": "{\"SearchPattern\": \"*Weapon*\"}"
}
```

### Search for Specific Class
```json
{
  "Action": "search",
  "ParamsJson": "{\"SearchPattern\": \"*\", \"ClassFilter\": \"Material\"}"
}
```

### Search in Specific Path
```json
{
  "Action": "search",
  "ParamsJson": "{\"SearchPattern\": \"*Enemy*\", \"Path\": \"/Game/Characters/Enemies\"}"
}
```

## Returns

```json
{
  "Success": true,
  "Assets": [
    {
      "Name": "BP_EnemyCharacter",
      "Path": "/Game/Characters/Enemies/BP_EnemyCharacter",
      "Class": "Blueprint"
    }
  ],
  "Count": 1
}
```

## Tips

- Use `*` as a wildcard to match any characters
- Class names are case-sensitive
- Search is performed across all loaded asset registries
- For faster searches, narrow down using both pattern and path filters
