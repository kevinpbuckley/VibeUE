# Runtime Virtual Texture (RVT) Setup Guide

## What is RVT?

Runtime Virtual Textures allow landscape materials to be rendered once into a virtual texture cache, then sampled cheaply by other materials (foliage, props, decals). This dramatically improves performance for complex landscape materials.

## Complete RVT Pipeline

### Step 1: Create the RVT Asset

```python
import unreal

result = unreal.RuntimeVirtualTextureService.create_runtime_virtual_texture(
    "RVT_Landscape_01",          # Asset name
    "/Game/VirtualTextures",      # Directory
    "BaseColor_Normal_Roughness", # MaterialType — must match material outputs
    256,                          # TileCount (powers of 2: 64, 128, 256, 512)
    256,                          # TileSize (pixels per tile: 128, 256, 512)
    4,                            # TileBorderSize (overlap for filtering)
    False,                        # bContinuousUpdate (True = re-render every frame)
    False                         # bSinglePhysicalSpace (True = no streaming, fits in one allocation)
)
print(f"Created RVT: {result.asset_path}")
```

### Step 2: Enable Virtual Texturing on Material

```python
import unreal

mat_path = "/Game/Materials/M_Landscape_Master"
unreal.MaterialService.set_material_property(mat_path, "bUsedWithVirtualTexturing", "true")
```

### Step 3: Add RVT Output Node to Material

```python
import unreal

mat_path = "/Game/Materials/M_Landscape_Master"

# Create RVT output expression
rvt_output = unreal.MaterialNodeService.create_expression(
    mat_path, "MaterialExpressionRuntimeVirtualTextureOutput", 400, 0)

# Connect your blended outputs to RVT pins
# These should be the same signals connected to material outputs
unreal.MaterialNodeService.connect_expressions(
    mat_path, blend_node_id, "RGB", rvt_output, "BaseColor")
unreal.MaterialNodeService.connect_expressions(
    mat_path, normal_node_id, "RGB", rvt_output, "Normal")
unreal.MaterialNodeService.connect_expressions(
    mat_path, roughness_node_id, "", rvt_output, "Roughness")

unreal.MaterialService.compile_material(mat_path)
```

### Step 4: Create RVT Volume Actor

The volume defines the world-space region cached by the RVT:

```python
import unreal

vol = unreal.RuntimeVirtualTextureService.create_rvt_volume(
    "MyLandscape",               # Landscape to cover
    "/Game/VirtualTextures/RVT_Landscape_01",
    "RVT_Volume_Landscape_01"    # Actor label
)
print(f"Volume: {vol.volume_label}")
```

### Step 5: Assign RVT to Landscape

```python
import unreal

unreal.RuntimeVirtualTextureService.assign_rvt_to_landscape(
    "MyLandscape",
    "/Game/VirtualTextures/RVT_Landscape_01",
    0  # Slot index (landscapes support multiple RVTs)
)
```

## RVT Material Types

| Type | Channels | Use Case |
|------|----------|----------|
| `BaseColor` | RGB | Color-only caching (simplest) |
| `BaseColor_Normal_Roughness` | RGB + Normal + Roughness | Standard landscape (most common) |
| `BaseColor_Normal_Specular` | RGB + Normal + Specular | For specular-heavy materials |
| `WorldHeight` | Height only | For distance-based effects, atmosphere |

**The RVT material type must match what your material actually outputs.** If you connect BaseColor + Normal + Roughness to the RVT output node, use `BaseColor_Normal_Roughness`.

## RVT Sizing Guidelines

| Landscape Size | TileCount | TileSize | Total Resolution |
|----------------|-----------|----------|-----------------|
| Small (1-4 km²) | 128 | 256 | 32K × 32K |
| Medium (4-16 km²) | 256 | 256 | 64K × 64K |
| Large (16+ km²) | 512 | 256 | 128K × 128K |

**Notes:**
- Higher tile count = more memory but better streaming
- `bContinuousUpdate = true` re-renders every frame (expensive, only for dynamic materials)
- `bSinglePhysicalSpace = true` disables streaming (entire VT in memory, fast but memory-heavy)

## Inspecting Existing RVTs

```python
import unreal

info = unreal.RuntimeVirtualTextureService.get_runtime_virtual_texture_info(
    "/Game/VirtualTextures/RVT_Landscape_01")

print(f"Type: {info.material_type}")
print(f"Tiles: {info.tile_count} × {info.tile_size}px")
print(f"Border: {info.tile_border_size}")
print(f"Continuous: {info.continuous_update}")
print(f"Single space: {info.single_physical_space}")
```

## Common Issues

### RVT Shows Black
- Material missing `bUsedWithVirtualTexturing = true`
- RVT output node not connected to matching pins
- No RVT Volume actor in the level

### RVT Shows Blurry/Low-Res
- Increase TileCount or TileSize
- Ensure volume bounds match landscape bounds
- Check if `bSinglePhysicalSpace` would help (if landscape is small enough)

### Performance Regression
- Don't set `bContinuousUpdate` unless material actually changes per-frame
- Consider smaller TileSize if memory-constrained
- Ensure volume isn't much larger than the landscape
