# remove

Remove one or more actors from the current level.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| actor_label | string | Yes* | Name of a single actor to remove |
| actor_labels | array | Yes* | Array of actor names to remove (batch mode) |
| actor_paths | array | Yes* | Array of full actor paths to remove (batch mode) |
| with_undo | bool | No | Whether to support undo (default: true) |

*One of `actor_label`, `actor_labels`, or `actor_paths` is required.

## Examples

### Remove Single Actor
```json
{
  "Action": "remove",
  "ParamsJson": "{\"actor_label\": \"PointLight_0\"}"
}
```

### Remove Multiple Actors by Label
```json
{
  "Action": "remove",
  "ParamsJson": "{\"actor_labels\": [\"TestLight1\", \"TestLight2\", \"TestCube\"]}"
}
```

### Remove Multiple Actors by Path
```json
{
  "Action": "remove",
  "ParamsJson": "{\"actor_paths\": [\"/Game/Level.Level:PersistentLevel.PointLight_0\", \"/Game/Level.Level:PersistentLevel.Cube_0\"]}"
}
```

## Returns

### Single Actor Response
```json
{
  "success": true,
  "actor": {
    "actor_label": "PointLight_0"
  }
}
```

### Batch Response
```json
{
  "success": true,
  "removed": [
    {"actor_label": "TestLight1", "success": true},
    {"actor_label": "TestLight2", "success": true}
  ],
  "failed": [],
  "removed_count": 2,
  "failed_count": 0
}
```

## Tips

- Use `actor_labels` array when removing multiple actors at once
- Removal supports undo by default - set `with_undo: false` to disable
- Child actors attached to removed actors will be detached
- Use `find` action to locate actors by name pattern first
- Save the level after removal to persist changes
