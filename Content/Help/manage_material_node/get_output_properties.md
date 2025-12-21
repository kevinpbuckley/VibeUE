# get_output_properties

Get available material output properties.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |

## Examples

```json
{
  "action": "get_output_properties",
  "material_path": "/Game/Materials/M_Test"
}
```

## Returns

- `success`: true/false
- `output_properties`: Array of available outputs
- `material_domain`: Material domain (Surface, PostProcess, etc.)
- `blend_mode`: Current blend mode

Each output property includes:
- `name`: Output name for connect_to_output
- `connected`: Whether currently connected
- `type`: Expected input type

## Common Output Properties

- `BaseColor` - Main surface color
- `Metallic` - Metal vs non-metal (0-1)
- `Specular` - Non-metal specular (usually 0.5)
- `Roughness` - Surface roughness (0-1)
- `EmissiveColor` - Self-illumination
- `Normal` - Normal map input
- `Opacity` - For translucent materials
- `OpacityMask` - For masked materials
- `AmbientOcclusion` - AO channel
- `WorldPositionOffset` - Vertex offset

## Tips

- Available outputs depend on material domain and blend mode
- Use this to see what outputs are valid for your material
- Outputs marked as connected already have expressions wired to them
