# refresh_editor

Refresh the Material Editor window if the material is currently open. Useful after programmatic changes.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |

## Examples

```json
{
  "action": "refresh_editor",
  "material_path": "/Game/Materials/MyMaterial"
}
```

## Returns

- `success`: true/false
- `message`: Success/error message

## Tips

- Only affects materials that are currently open in the Material Editor
- Useful after making node graph changes programmatically
- If material is not open, this action does nothing but still succeeds
