# compile

Recompile material shaders. Required after making property changes for them to take effect visually.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |

## Examples

```json
{
  "action": "compile",
  "material_path": "/Game/Materials/MyMaterial"
}
```

## Returns

- `success`: true/false
- `message`: Success/error message

## Tips

- Compilation is required after changing blend mode, shading model, or other shader-affecting properties
- Material node changes also require recompilation
- Large materials may take a moment to compile
- Check the output log for any compilation errors
