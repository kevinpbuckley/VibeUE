# attach

Attach an actor to another actor as a child. The child actor will follow the parent's transforms.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| child_label | string | Yes* | Label of the child actor to attach |
| parent_label | string | Yes* | Label of the parent actor |
| socket_name | string | No | Socket to attach to (for skeletal meshes) |
| weld_simulated_bodies | bool | No | Whether to weld physics bodies (default: false) |

*Alternative identifiers: `child_path`, `child_guid`, `child_tag` for child; `parent_path`, `parent_guid`, `parent_tag` for parent.

## Examples

### Basic Attachment
```json
{
  "Action": "attach",
  "ParamsJson": "{\"child_label\": \"Weapon\", \"parent_label\": \"BP_Player_C_0\"}"
}
```

### Attach to Socket
```json
{
  "Action": "attach",
  "ParamsJson": "{\"child_label\": \"Sword\", \"parent_label\": \"Character\", \"socket_name\": \"hand_r\"}"
}
```

### Attach Light to Platform
```json
{
  "Action": "attach",
  "ParamsJson": "{\"child_label\": \"PointLight1\", \"parent_label\": \"MovingPlatform\"}"
}
```

## Returns

```json
{
  "success": true,
  "actor": {
    "actor_label": "Weapon",
    "class_name": "StaticMeshActor",
    "parent": "BP_Player_C_0"
  }
}
```

## Error Responses

| Error Code | Description |
|------------|-------------|
| INVALID_CHILD | No child actor identifier provided or child not found |
| INVALID_PARENT | No parent actor identifier provided or parent not found |
| ATTACH_FAILED | Failed to attach the actors |

## Tips

- Attached actors follow parent transforms automatically
- Use sockets for precise attachment to skeletal meshes
- Common for attaching weapons, lights, effects to characters/vehicles
- Child maintains its relative offset to parent
- Detach with the `detach` action when needed
