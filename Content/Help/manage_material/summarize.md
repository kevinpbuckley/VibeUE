# summarize

Get a brief summary of a material's key properties.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |

## Examples

```json
{
  "action": "summarize",
  "material_path": "/Game/Materials/MyMaterial"
}
```

## Returns

- `success`: true/false
- `summary`: Brief text summary of material
- Key properties like blend mode, shading model, etc.

## Tips

- Faster than `get_info` for quick overview
- Use when you just need to know the basics
- Use `get_info` for comprehensive details
