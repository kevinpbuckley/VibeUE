# export_texture

Export a texture asset from the project to an external file.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| AssetPath | string | Yes | Content path to the texture asset to export |
| DestinationPath | string | Yes | Full path where the texture file will be saved |
| Format | string | No | Export format: "PNG", "TGA", "JPG", "EXR" (default: "PNG") |

## Examples

### Export as PNG
```json
{
  "Action": "export_texture",
  "ParamsJson": "{\"AssetPath\": \"/Game/Textures/Characters/T_Hero_Diffuse\", \"DestinationPath\": \"C:/Export/hero_diffuse.png\"}"
}
```

### Export as TGA
```json
{
  "Action": "export_texture",
  "ParamsJson": "{\"AssetPath\": \"/Game/Textures/Environment/T_Ground_Normal\", \"DestinationPath\": \"C:/Export/ground_normal.tga\", \"Format\": \"TGA\"}"
}
```

### Export HDR Texture
```json
{
  "Action": "export_texture",
  "ParamsJson": "{\"AssetPath\": \"/Game/Textures/HDRI/T_Sky_HDR\", \"DestinationPath\": \"C:/Export/sky.exr\", \"Format\": \"EXR\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ExportedPath": "C:/Export/hero_diffuse.png",
  "Message": "Texture exported successfully"
}
```

## Tips

- PNG format preserves alpha channel and is lossless
- TGA is preferred for textures that will be re-imported
- EXR format is best for HDR and high bit-depth textures
- JPG does not support alpha channel and uses lossy compression
- Ensure the destination directory exists before exporting
