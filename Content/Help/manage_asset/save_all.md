# save_all

Save all modified assets in the project.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| OnlyIfLoaded | boolean | No | Only save assets currently loaded in memory (default: true) |

## Examples

### Save All Modified Assets
```json
{
  "Action": "save_all",
  "ParamsJson": "{}"
}
```

### Save All Loaded Assets
```json
{
  "Action": "save_all",
  "ParamsJson": "{\"OnlyIfLoaded\": true}"
}
```

## Returns

```json
{
  "Success": true,
  "SavedCount": 12,
  "Message": "12 assets saved successfully"
}
```

## Tips

- This is equivalent to pressing Ctrl+Shift+S in the editor
- Large projects may take time to save all assets
- Saving compiles any dirty blueprints or materials
- Use after batch operations to ensure all changes are persisted
- Consider saving periodically during long editing sessions
