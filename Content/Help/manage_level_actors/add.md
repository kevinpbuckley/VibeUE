# add

Spawn a new actor in the current level.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| Class | string | Yes | Class of actor to spawn (e.g., "PointLight", "StaticMeshActor", "BP_Enemy") |
| Location | object | No | Spawn location {"X": 0, "Y": 0, "Z": 0} |
| Rotation | object | No | Spawn rotation {"Pitch": 0, "Yaw": 0, "Roll": 0} |
| Scale | object | No | Spawn scale {"X": 1, "Y": 1, "Z": 1} |
| Name | string | No | Custom name for the actor |
| Folder | string | No | Folder path to place the actor in |

## Examples

### Add Point Light
```json
{
  "Action": "add",
  "ParamsJson": "{\"Class\": \"PointLight\", \"Location\": {\"X\": 0, \"Y\": 0, \"Z\": 200}}"
}
```

### Add Static Mesh Actor
```json
{
  "Action": "add",
  "ParamsJson": "{\"Class\": \"StaticMeshActor\", \"Location\": {\"X\": 100, \"Y\": 0, \"Z\": 0}, \"Name\": \"FloorMesh\"}"
}
```

### Add Blueprint Actor
```json
{
  "Action": "add",
  "ParamsJson": "{\"Class\": \"/Game/Blueprints/BP_Enemy\", \"Location\": {\"X\": 500, \"Y\": 0, \"Z\": 100}, \"Rotation\": {\"Pitch\": 0, \"Yaw\": 180, \"Roll\": 0}}"
}
```

### Add Actor with Folder
```json
{
  "Action": "add",
  "ParamsJson": "{\"Class\": \"SpotLight\", \"Location\": {\"X\": 0, \"Y\": 0, \"Z\": 300}, \"Folder\": \"Lighting/Indoor\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "PointLight_0",
  "Class": "PointLight",
  "Location": {"X": 0, "Y": 0, "Z": 200},
  "Message": "Actor spawned successfully"
}
```

## Tips

- Common classes: PointLight, SpotLight, DirectionalLight, StaticMeshActor, CameraActor, PlayerStart
- Blueprint actors use their full content path
- Names are auto-generated if not specified
- Location defaults to world origin if not provided
- Use folders to organize actors in complex levels
