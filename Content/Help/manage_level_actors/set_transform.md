# set_transform

Set the full transform (location, rotation, scale) of an actor.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor to transform |
| Location | object | No | New location {"X": 0, "Y": 0, "Z": 0} |
| Rotation | object | No | New rotation {"Pitch": 0, "Yaw": 0, "Roll": 0} |
| Scale | object | No | New scale {"X": 1, "Y": 1, "Z": 1} |
| WorldSpace | boolean | No | Use world space (default: true) |

## Examples

### Set Full Transform
```json
{
  "Action": "set_transform",
  "ParamsJson": "{\"ActorName\": \"StaticMesh_0\", \"Location\": {\"X\": 100, \"Y\": 200, \"Z\": 50}, \"Rotation\": {\"Pitch\": 0, \"Yaw\": 45, \"Roll\": 0}, \"Scale\": {\"X\": 2, \"Y\": 2, \"Z\": 2}}"
}
```

### Set Location and Rotation Only
```json
{
  "Action": "set_transform",
  "ParamsJson": "{\"ActorName\": \"PointLight_0\", \"Location\": {\"X\": 0, \"Y\": 0, \"Z\": 300}, \"Rotation\": {\"Pitch\": -90, \"Yaw\": 0, \"Roll\": 0}}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "StaticMesh_0",
  "Transform": {
    "Location": {"X": 100, "Y": 200, "Z": 50},
    "Rotation": {"Pitch": 0, "Yaw": 45, "Roll": 0},
    "Scale": {"X": 2, "Y": 2, "Z": 2}
  },
  "Message": "Transform set successfully"
}
```

## Tips

- Only specify the components you want to change
- Rotation uses degrees (Pitch, Yaw, Roll)
- Scale of 1 is default size
- WorldSpace=false uses parent-relative coordinates
- Attached children move with the parent
