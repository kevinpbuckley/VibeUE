# open

Open a material or material instance in the Material Editor.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material or material instance |

## Examples

### Open a Material
```json
{
  "action": "open",
  "material_path": "/Game/Materials/MyMaterial"
}
```

### Open a Material Instance
```json
{
  "action": "open",
  "material_path": "/Game/Materials/MI_MyMaterialInstance"
}
```

## Returns

- `success`: true/false
- `material_path`: The path of the opened material
- `message`: Success/error message

## Tips

- Works with both base materials (UMaterial) and material instances (UMaterialInstance)
- Opens the asset in the appropriate editor (Material Editor for materials, Material Instance Editor for instances)
- Use this before making node changes with `manage_material_node`
