# attach

Attach an actor to another actor as a child.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor to attach |
| ParentName | string | Yes | Name of the parent actor |
| SocketName | string | No | Socket to attach to (for skeletal meshes) |
| AttachmentRule | string | No | "KeepRelative", "KeepWorld", or "SnapToTarget" (default: "KeepWorld") |

## Examples

### Basic Attachment
```json
{
  "Action": "attach",
  "ParamsJson": "{\"ActorName\": \"Weapon\", \"ParentName\": \"BP_Player_C_0\"}"
}
```

### Attach to Socket
```json
{
  "Action": "attach",
  "ParamsJson": "{\"ActorName\": \"Weapon\", \"ParentName\": \"BP_Player_C_0\", \"SocketName\": \"hand_r\"}"
}
```

### Attach with Rule
```json
{
  "Action": "attach",
  "ParamsJson": "{\"ActorName\": \"Light\", \"ParentName\": \"Platform\", \"AttachmentRule\": \"KeepRelative\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "Weapon",
  "ParentName": "BP_Player_C_0",
  "Message": "Actor attached successfully"
}
```

## Tips

- KeepWorld: Maintains world position during attachment
- KeepRelative: Maintains relative offset to parent
- SnapToTarget: Moves to parent's position
- Attached actors follow parent transforms
- Use sockets for precise skeletal mesh attachment points
