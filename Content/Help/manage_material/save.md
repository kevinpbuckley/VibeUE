# save

Save a material asset to disk.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |

## Examples

```json
{
  "action": "save",
  "material_path": "/Game/Materials/MyMaterial"
}
```

## Returns

- `success`: true/false
- `message`: Success/error message

## Tips

- Always save after making changes you want to persist
- Unsaved changes will be lost when the editor closes
- Use `save_all` from `manage_asset` to save multiple assets at once
