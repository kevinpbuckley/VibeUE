# create

Create a new material asset at the specified path.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| destination_path | string | Yes | Package path like `/Game/Materials/MyFolder` |
| material_name | string | Yes | Name for the new material |
| initial_properties | object | No | Optional initial property values as key-value pairs |

## Examples

Create a basic material:
```json
{
  "action": "create",
  "destination_path": "/Game/Materials/test",
  "material_name": "MyMaterial"
}
```

Create with initial properties:
```json
{
  "action": "create",
  "destination_path": "/Game/Materials/test",
  "material_name": "MyMaterial",
  "initial_properties": {
    "TwoSided": true,
    "BlendMode": "BLEND_Masked"
  }
}
```

## Returns

- `success`: true/false
- `material_path`: Full path to created material
- `message`: Success/error message

## Tips

- The destination path should be a valid content folder path starting with `/Game/`
- Material names should not contain special characters
- Use `compile` action after creation to compile shaders
