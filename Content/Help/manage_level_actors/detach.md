# detach

Detach an actor from its parent, making it an independent actor in the level.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| actor_label | string | Yes* | Label of the actor to detach from its parent |

*Alternative identifiers: `actor_path`, `actor_guid`, `actor_tag`

## Examples

### Detach Actor
```json
{
  "Action": "detach",
  "ParamsJson": "{\"actor_label\": \"Weapon\"}"
}
```

### Detach by GUID
```json
{
  "Action": "detach",
  "ParamsJson": "{\"actor_guid\": \"ABC123DEF456\"}"
}
```

## Returns

```json
{
  "success": true,
  "actor": {
    "actor_label": "Weapon",
    "class_name": "StaticMeshActor",
    "previous_parent": "BP_Player_C_0"
  }
}
```

## Error Responses

| Error Code | Description |
|------------|-------------|
| INVALID_IDENTIFIER | No actor identifier provided |
| ACTOR_NOT_FOUND | Actor with given identifier not found |
| DETACH_FAILED | Failed to detach the actor |

## Tips

- Detached actors maintain their world position
- Actor becomes independent in World Outliner
- Use when you need to free an attached object
- Commonly used for drop/throw mechanics
