# set_rotation

Set only the rotation of an actor, preserving location and scale.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor |
| Rotation | object | Yes | New rotation {"Pitch": 0, "Yaw": 0, "Roll": 0} |
| WorldSpace | boolean | No | Use world space (default: true) |

## Examples

### Set World Rotation
```json
{
  "Action": "set_rotation",
  "ParamsJson": "{\"ActorName\": \"SpotLight_0\", \"Rotation\": {\"Pitch\": -90, \"Yaw\": 0, \"Roll\": 0}}"
}
```

### Rotate Actor to Face Direction
```json
{
  "Action": "set_rotation",
  "ParamsJson": "{\"ActorName\": \"BP_Enemy_0\", \"Rotation\": {\"Pitch\": 0, \"Yaw\": 180, \"Roll\": 0}}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "SpotLight_0",
  "Rotation": {"Pitch": -90, "Yaw": 0, "Roll": 0},
  "Message": "Rotation set successfully"
}
```

## Tips

- Rotation uses degrees
- Pitch: up/down tilt, Yaw: left/right turn, Roll: sideways tilt
- Pitch -90 points straight down
- Yaw 0 faces forward (+X direction)
- Preserves existing location and scale
