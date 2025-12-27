# set_folder

Set the folder path for an actor in the World Outliner.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor |
| Folder | string | Yes | Folder path (use "/" for nested folders, empty string for root) |

## Examples

### Move to Folder
```json
{
  "Action": "set_folder",
  "ParamsJson": "{\"ActorName\": \"PointLight_0\", \"Folder\": \"Lighting\"}"
}
```

### Move to Nested Folder
```json
{
  "Action": "set_folder",
  "ParamsJson": "{\"ActorName\": \"SpotLight_0\", \"Folder\": \"Lighting/Indoor/Kitchen\"}"
}
```

### Move to Root
```json
{
  "Action": "set_folder",
  "ParamsJson": "{\"ActorName\": \"PlayerStart\", \"Folder\": \"\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "PointLight_0",
  "Folder": "Lighting",
  "Message": "Actor folder set successfully"
}
```

## Tips

- Folders are organizational only - they don't affect gameplay
- Use forward slashes for nested folders
- Empty string moves to root level
- Folders are created automatically if they don't exist
- Good organization improves level editing workflow
