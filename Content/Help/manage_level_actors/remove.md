# remove

Remove an actor from the current level.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor to remove |

## Examples

### Remove Actor
```json
{
  "Action": "remove",
  "ParamsJson": "{\"ActorName\": \"PointLight_0\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "PointLight_0",
  "Message": "Actor removed successfully"
}
```

## Tips

- Removal is immediate and cannot be undone through the tool
- Child actors attached to this actor will be detached
- Use `find` action to locate actors by name pattern first
- Save the level after removal to persist changes
