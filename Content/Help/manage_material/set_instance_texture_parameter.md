# set_instance_texture_parameter

Set or override a texture parameter on a material instance.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| instance_path | string | Yes | Full path to material instance |
| parameter_name | string | Yes | Name of texture parameter |
| texture_path | string | Yes | Path to texture asset |

## Examples

```json
{
  "action": "set_instance_texture_parameter",
  "instance_path": "/Game/Materials/Instances/MI_RedVariant",
  "parameter_name": "BaseColorTexture",
  "texture_path": "/Game/Textures/T_Brick_D"
}
```

## Returns

- `success`: true/false
- `parameter_name`: Parameter that was set
- `texture_path`: Path to assigned texture
- `message`: Success/error message

## Tips

- Texture must exist at specified path
- Parameter must exist in parent material
- Use `manage_asset` with `search` action to find textures
- Setting to empty string or null may clear the override
