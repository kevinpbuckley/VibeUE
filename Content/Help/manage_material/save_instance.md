# save_instance

Save a material instance to disk.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| instance_path | string | Yes | Full path to material instance |

## Examples

```json
{
  "action": "save_instance",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant"
}
```

## Returns

- `success`: true/false
- `message`: Success/error message

## Tips

- Always save after making parameter override changes
- Unsaved changes will be lost when the editor closes
- Use `save_all` from `manage_asset` to save multiple assets at once
