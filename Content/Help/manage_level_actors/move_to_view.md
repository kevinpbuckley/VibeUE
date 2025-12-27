# move_to_view

Move an actor to the current camera view location.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor to move |

## Examples

### Move Actor to Current View
```json
{
  "Action": "move_to_view",
  "ParamsJson": "{\"ActorName\": \"PointLight_0\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "PointLight_0",
  "NewLocation": {"X": 250, "Y": -100, "Z": 180},
  "Message": "Actor moved to current view"
}
```

## Tips

- Moves actor to where the camera is looking
- Rotation is not changed by default
- Useful for quick placement from camera perspective
- Position your viewport first, then call this action
