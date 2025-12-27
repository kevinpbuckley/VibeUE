# list

List all actors in the current level.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ClassFilter | string | No | Filter by actor class |
| Folder | string | No | Filter by folder path |
| MaxResults | number | No | Maximum number of results (default: 1000) |

## Examples

### List All Actors
```json
{
  "Action": "list",
  "ParamsJson": "{}"
}
```

### List Only Lights
```json
{
  "Action": "list",
  "ParamsJson": "{\"ClassFilter\": \"Light\"}"
}
```

### List Actors in Folder
```json
{
  "Action": "list",
  "ParamsJson": "{\"Folder\": \"Lighting\"}"
}
```

## Returns

```json
{
  "Success": true,
  "Actors": [
    {
      "Name": "Floor",
      "Class": "StaticMeshActor",
      "Location": {"X": 0, "Y": 0, "Z": 0},
      "Folder": ""
    },
    {
      "Name": "PointLight_0",
      "Class": "PointLight",
      "Location": {"X": 0, "Y": 0, "Z": 200},
      "Folder": "Lighting"
    },
    {
      "Name": "BP_Player_C_0",
      "Class": "BP_Player_C",
      "Location": {"X": 100, "Y": 0, "Z": 100},
      "Folder": "Gameplay"
    }
  ],
  "Count": 3
}
```

## Tips

- Large levels may have many actors; use filters to narrow results
- ClassFilter matches partial class names
- Hidden actors are included in the list
- Use `get_info` for detailed information about specific actors
