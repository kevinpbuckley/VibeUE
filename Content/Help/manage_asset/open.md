# open

Open an asset in its appropriate editor.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| AssetPath | string | Yes | Content path to the asset to open |

## Examples

### Open Blueprint
```json
{
  "Action": "open",
  "ParamsJson": "{\"AssetPath\": \"/Game/Blueprints/BP_Player\"}"
}
```

### Open Material
```json
{
  "Action": "open",
  "ParamsJson": "{\"AssetPath\": \"/Game/Materials/M_Metal\"}"
}
```

### Open Level
```json
{
  "Action": "open",
  "ParamsJson": "{\"AssetPath\": \"/Game/Maps/MainMenu\"}"
}
```

## Returns

```json
{
  "Success": true,
  "AssetPath": "/Game/Blueprints/BP_Player",
  "Message": "Asset opened in editor"
}
```

## Tips

- Assets open in their type-specific editor (Blueprint Editor, Material Editor, etc.)
- Opening a level will switch the current level in the viewport
- Multiple assets can be open simultaneously in separate tabs
- Opening an already-open asset brings its editor tab to focus
- Some asset types open in external applications (e.g., skeletal meshes in Persona)
