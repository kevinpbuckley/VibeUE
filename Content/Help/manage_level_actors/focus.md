# focus

Focus the viewport camera on an actor.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor to focus on |

## Examples

### Focus on Actor
```json
{
  "Action": "focus",
  "ParamsJson": "{\"ActorName\": \"BP_Player_C_0\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "BP_Player_C_0",
  "Message": "Viewport focused on actor"
}
```

## Tips

- Similar to pressing F in the editor with actor selected
- Camera zooms to fit the actor in view
- Useful for navigating to specific actors in large levels
- Works in all viewport modes
