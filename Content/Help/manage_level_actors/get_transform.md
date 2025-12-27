# get_transform

Get the current transform of an actor.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor |
| WorldSpace | boolean | No | Get world space transform (default: true) |

## Examples

### Get World Transform
```json
{
  "Action": "get_transform",
  "ParamsJson": "{\"ActorName\": \"BP_Player_C_0\"}"
}
```

### Get Relative Transform
```json
{
  "Action": "get_transform",
  "ParamsJson": "{\"ActorName\": \"AttachedMesh\", \"WorldSpace\": false}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "BP_Player_C_0",
  "Transform": {
    "Location": {"X": 100, "Y": 0, "Z": 100},
    "Rotation": {"Pitch": 0, "Yaw": 90, "Roll": 0},
    "Scale": {"X": 1, "Y": 1, "Z": 1}
  },
  "WorldSpace": true
}
```

## Tips

- WorldSpace=true gives absolute world position
- WorldSpace=false gives position relative to parent
- Useful for calculating offsets or duplicating positions
