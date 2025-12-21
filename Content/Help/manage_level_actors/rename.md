# rename

Rename an actor in the level.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Current name of the actor |
| NewName | string | Yes | New name for the actor |

## Examples

### Rename Actor
```json
{
  "Action": "rename",
  "ParamsJson": "{\"ActorName\": \"StaticMeshActor_0\", \"NewName\": \"Floor_Main\"}"
}
```

### Rename with Descriptive Name
```json
{
  "Action": "rename",
  "ParamsJson": "{\"ActorName\": \"PointLight_3\", \"NewName\": \"KitchenCeilingLight\"}"
}
```

## Returns

```json
{
  "Success": true,
  "OldName": "StaticMeshActor_0",
  "NewName": "Floor_Main",
  "Message": "Actor renamed successfully"
}
```

## Tips

- Actor names must be unique in the level
- Use descriptive names for easier level management
- Renaming doesn't affect level loading or gameplay
- References to the actor use the new name after rename
- Save the level to persist the rename
