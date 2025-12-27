# set_location

Set only the location of an actor, preserving rotation and scale.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor |
| Location | object | Yes | New location {"X": 0, "Y": 0, "Z": 0} |
| WorldSpace | boolean | No | Use world space (default: true) |

## Examples

### Set World Location
```json
{
  "Action": "set_location",
  "ParamsJson": "{\"ActorName\": \"PointLight_0\", \"Location\": {\"X\": 100, \"Y\": 200, \"Z\": 300}}"
}
```

### Set Relative Location
```json
{
  "Action": "set_location",
  "ParamsJson": "{\"ActorName\": \"ChildActor\", \"Location\": {\"X\": 50, \"Y\": 0, \"Z\": 0}, \"WorldSpace\": false}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "PointLight_0",
  "Location": {"X": 100, "Y": 200, "Z": 300},
  "Message": "Location set successfully"
}
```

## Tips

- More efficient than set_transform when only moving
- Preserves existing rotation and scale
- WorldSpace=false is useful for attached actors
