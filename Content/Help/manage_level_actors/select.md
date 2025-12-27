# select

Select actors in the editor viewport.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorNames | array | Yes | Array of actor names to select |
| AddToSelection | boolean | No | Add to current selection (default: false, replaces selection) |

## Examples

### Select Single Actor
```json
{
  "Action": "select",
  "ParamsJson": "{\"ActorNames\": [\"BP_Player_C_0\"]}"
}
```

### Select Multiple Actors
```json
{
  "Action": "select",
  "ParamsJson": "{\"ActorNames\": [\"PointLight_0\", \"PointLight_1\", \"PointLight_2\"]}"
}
```

### Add to Selection
```json
{
  "Action": "select",
  "ParamsJson": "{\"ActorNames\": [\"StaticMesh_5\"], \"AddToSelection\": true}"
}
```

## Returns

```json
{
  "Success": true,
  "SelectedActors": ["PointLight_0", "PointLight_1", "PointLight_2"],
  "SelectionCount": 3,
  "Message": "Actors selected"
}
```

## Tips

- Selection is shown in the viewport and Details panel
- Empty array clears selection
- AddToSelection=true is like Ctrl+Click
- Useful for batch operations in the editor
