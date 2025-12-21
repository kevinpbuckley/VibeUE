# detach

Detach an actor from its parent.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor to detach |
| DetachmentRule | string | No | "KeepRelative" or "KeepWorld" (default: "KeepWorld") |

## Examples

### Detach Actor
```json
{
  "Action": "detach",
  "ParamsJson": "{\"ActorName\": \"Weapon\"}"
}
```

### Detach Keeping Relative Transform
```json
{
  "Action": "detach",
  "ParamsJson": "{\"ActorName\": \"Attachment\", \"DetachmentRule\": \"KeepRelative\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "Weapon",
  "PreviousParent": "BP_Player_C_0",
  "Message": "Actor detached successfully"
}
```

## Tips

- KeepWorld maintains the actor's world position
- KeepRelative maintains the transform values (which will change its world position)
- Detached actors become independent world actors
- Actor is moved to root level in World Outliner
