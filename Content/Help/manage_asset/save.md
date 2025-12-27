# save

Save a specific asset to disk.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| AssetPath | string | Yes | Content path to the asset to save |

## Examples

### Save Single Asset
```json
{
  "Action": "save",
  "ParamsJson": "{\"AssetPath\": \"/Game/Blueprints/BP_Player\"}"
}
```

### Save Material
```json
{
  "Action": "save",
  "ParamsJson": "{\"AssetPath\": \"/Game/Materials/M_Character_Skin\"}"
}
```

## Returns

```json
{
  "Success": true,
  "AssetPath": "/Game/Blueprints/BP_Player",
  "Message": "Asset saved successfully"
}
```

## Tips

- Always save assets after making modifications to persist changes
- Unsaved assets are indicated with an asterisk (*) in the editor
- Saving triggers any necessary asset compilation (materials, blueprints)
- Use `save_all` to batch save multiple modified assets
- Assets must be saved before they can be used in packaged builds
