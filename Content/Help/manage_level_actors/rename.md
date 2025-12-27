# rename

Rename an actor's label in the level outliner.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| actor_label | string | Yes* | Current label of the actor to rename |
| new_label | string | Yes | New label for the actor |

*Alternative identifiers: `actor_path`, `actor_guid`, `actor_tag`

## Examples

### Rename Actor
```json
{
  "Action": "rename",
  "ParamsJson": "{\"actor_label\": \"StaticMeshActor_0\", \"new_label\": \"Floor_Main\"}"
}
```

### Rename with Descriptive Name
```json
{
  "Action": "rename",
  "ParamsJson": "{\"actor_label\": \"PointLight_3\", \"new_label\": \"KitchenCeilingLight\"}"
}
```

### Rename by GUID
```json
{
  "Action": "rename",
  "ParamsJson": "{\"actor_guid\": \"ABC123\", \"new_label\": \"MainEntrance\"}"
}
```

## Returns

```json
{
  "success": true,
  "actor": {
    "actor_label": "Floor_Main",
    "old_label": "StaticMeshActor_0",
    "class_name": "StaticMeshActor"
  }
}
```

## Error Responses

| Error Code | Description |
|------------|-------------|
| INVALID_IDENTIFIER | No actor identifier provided |
| ACTOR_NOT_FOUND | Actor with given identifier not found |
| EMPTY_LABEL | New label cannot be empty |
| RENAME_FAILED | Failed to rename the actor |

## Tips

- Actor labels must be unique in the level
- Use descriptive names for easier level management
- Renaming doesn't affect level loading or gameplay logic
- Save the level to persist the rename
