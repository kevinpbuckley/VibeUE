# import_texture

Import an external texture file into the Unreal Engine project.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| SourcePath | string | Yes | Full path to the source texture file on disk |
| DestinationPath | string | Yes | Content folder path where the texture will be imported |
| AssetName | string | No | Custom name for the imported asset (defaults to source filename) |
| CompressionSettings | string | No | Texture compression type (e.g., "Default", "Normalmap", "Masks") |
| SRGB | boolean | No | Whether the texture uses sRGB color space (default: true) |

## Examples

### Basic Import
```json
{
  "Action": "import_texture",
  "ParamsJson": "{\"SourcePath\": \"C:/Art/Textures/wall_brick_diffuse.png\", \"DestinationPath\": \"/Game/Textures/Environment\"}"
}
```

### Import Normal Map
```json
{
  "Action": "import_texture",
  "ParamsJson": "{\"SourcePath\": \"C:/Art/Textures/wall_brick_normal.png\", \"DestinationPath\": \"/Game/Textures/Environment\", \"CompressionSettings\": \"Normalmap\", \"SRGB\": false}"
}
```

### Import with Custom Name
```json
{
  "Action": "import_texture",
  "ParamsJson": "{\"SourcePath\": \"C:/Downloads/texture_001.png\", \"DestinationPath\": \"/Game/Textures/Props\", \"AssetName\": \"T_Barrel_Diffuse\"}"
}
```

## Returns

```json
{
  "Success": true,
  "AssetPath": "/Game/Textures/Environment/wall_brick_diffuse",
  "Message": "Texture imported successfully"
}
```

## Tips

- Supported formats: PNG, TGA, JPG, BMP, PSD, EXR, HDR
- Normal maps should have SRGB disabled and use Normalmap compression
- Mask textures (roughness, metallic, AO) should use Masks compression
- Power-of-two dimensions (512, 1024, 2048) are recommended for mipmaps
- The destination folder will be created if it doesn't exist
