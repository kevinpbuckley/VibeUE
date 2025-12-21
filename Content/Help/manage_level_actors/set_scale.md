# set_scale

Set only the scale of an actor, preserving location and rotation.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor |
| Scale | object | Yes | New scale {"X": 1, "Y": 1, "Z": 1} |

## Examples

### Uniform Scale
```json
{
  "Action": "set_scale",
  "ParamsJson": "{\"ActorName\": \"StaticMesh_0\", \"Scale\": {\"X\": 2, \"Y\": 2, \"Z\": 2}}"
}
```

### Non-Uniform Scale
```json
{
  "Action": "set_scale",
  "ParamsJson": "{\"ActorName\": \"Platform\", \"Scale\": {\"X\": 5, \"Y\": 5, \"Z\": 0.5}}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "StaticMesh_0",
  "Scale": {"X": 2, "Y": 2, "Z": 2},
  "Message": "Scale set successfully"
}
```

## Tips

- Scale of 1 is the original size
- Non-uniform scaling may cause visual artifacts on some meshes
- Scale affects collision as well as visual size
- Preserves existing location and rotation
