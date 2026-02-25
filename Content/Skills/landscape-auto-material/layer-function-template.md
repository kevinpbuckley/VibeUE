# Layer Function Template

## Overview

A layer function encapsulates all texture sampling logic for a single terrain type (grass, rock, snow, etc.). This makes per-terrain materials modular and reusable across different master materials.

## Template: Creating a New Layer Function

### Step 1: Create Function Asset

```python
import unreal

func = unreal.MaterialNodeService.create_material_function(
    "MF_Layer_<TerrainType>",        # e.g., MF_Layer_Tropical
    "/Game/Materials/Functions/Layers",
    "Samples <TerrainType> terrain textures with tiling and detail blending",
    True       # bExposeToLibrary
)
func_path = func.asset_path
```

### Step 2: Define Interface

**Standard inputs** (every layer function should have these):

```python
# Texture inputs
unreal.MaterialNodeService.add_function_input(
    func_path, "BaseColorTexture", "Texture2D", 0,
    "Albedo/diffuse texture for this terrain type")
unreal.MaterialNodeService.add_function_input(
    func_path, "NormalTexture", "Texture2D", 1,
    "Normal map texture")
unreal.MaterialNodeService.add_function_input(
    func_path, "RoughnessTexture", "Texture2D", 2,
    "Roughness texture (or RMA packed)")

# Tiling control
unreal.MaterialNodeService.add_function_input(
    func_path, "UVTiling", "Scalar", 3,
    "UV tiling scale (default 0.01 for landscapes)")

# Optional: detail textures for close-up
unreal.MaterialNodeService.add_function_input(
    func_path, "DetailBaseColor", "Texture2D", 10,
    "Close-up detail texture (optional)")
unreal.MaterialNodeService.add_function_input(
    func_path, "DetailScale", "Scalar", 11,
    "Detail texture tiling multiplier")
```

**Standard outputs:**

```python
unreal.MaterialNodeService.add_function_output(
    func_path, "BaseColor", 0, "Sampled albedo (RGB)")
unreal.MaterialNodeService.add_function_output(
    func_path, "Normal", 1, "Sampled normal (RGB)")  
unreal.MaterialNodeService.add_function_output(
    func_path, "Roughness", 2, "Sampled roughness (Scalar)")
```

### Step 3: Build Internal Graph

Inside the function, the typical node chain is:

```
LandscapeLayerCoords (UV tiling)
    ↓
TextureSample (BaseColor) → Output: BaseColor
TextureSample (Normal)    → Output: Normal  
TextureSample (Roughness) → Output: Roughness
    ↓ (optional)
DetailTextureSample → Lerp with base by distance → Final outputs
```

```python
# Create UV tiling node
coords_id = unreal.MaterialNodeService.create_function_expression(
    func_path, "MaterialExpressionLandscapeLayerCoords", -600, 0)

# Create texture samplers
bc_sample = unreal.MaterialNodeService.create_function_expression(
    func_path, "MaterialExpressionTextureSample", -300, -100)
n_sample = unreal.MaterialNodeService.create_function_expression(
    func_path, "MaterialExpressionTextureSample", -300, 100)
r_sample = unreal.MaterialNodeService.create_function_expression(
    func_path, "MaterialExpressionTextureSample", -300, 300)

# Wire UV coords to samplers
unreal.MaterialNodeService.connect_function_expressions(
    func_path, coords_id, "", bc_sample, "UVs")
unreal.MaterialNodeService.connect_function_expressions(
    func_path, coords_id, "", n_sample, "UVs")
unreal.MaterialNodeService.connect_function_expressions(
    func_path, coords_id, "", r_sample, "UVs")

unreal.EditorAssetLibrary.save_asset(func_path)
```

### Step 4: Wire Into Master Material

```python
mat_path = "/Game/Materials/M_Landscape_Master"

# Create function call node
call_id = unreal.MaterialNodeService.create_expression(
    mat_path, "MaterialExpressionMaterialFunctionCall", -600, 0)

# Set function reference
unreal.MaterialNodeService.set_expression_property(
    mat_path, call_id, "MaterialFunction",
    f"MaterialFunction'{func_path}.MF_Layer_<TerrainType>'")

# Connect outputs to layer blend inputs
unreal.MaterialNodeService.connect_expressions(
    mat_path, call_id, "BaseColor", blend_id, "<TerrainType> BaseColor")
```

## Example Layer Function Catalog

| Function | Terrain | Textures | Special Features |
|----------|---------|----------|-----------------|
| `MF_Layer_Grass` | Grassland | BC + N + H | Color variation |
| `MF_Layer_Rock` | Rocky surfaces | BC + N + H | World-aligned UV |
| `MF_Layer_Snow` | Snow/ice | BC + N | Sparkle overlay |
| `MF_Layer_Dirt` | Bare earth | BC + N + H | Moisture darkening |
| `MF_Layer_Forest` | Forest floor | BC + N + H | Leaf litter blend |
| `MF_Layer_Beach` | Sand/beach | BC + N | Wet/dry transition |
| `MF_Layer_Desert` | Desert sand | BC + N + H | Wind ripple normal |
| `MF_Layer_Grass_Dry` | Dry grass | BC + N | Seasonal variant |
| `MF_Layer_Rock_Desert` | Desert rock | BC + N + H | Eroded variant |
| `MF_Layer_ToDuplicateToCreateNew` | Template | Empty | Copy and customize |

## Tips

- **Keep functions focused**: One terrain type per function, one concern per function
- **Use consistent pin naming**: `BaseColor`, `Normal`, `Roughness` for all output functions
- **Sort priorities matter**: Keep them consistent across all layer functions (0=BC, 1=Normal, 2=Roughness, 3=UV, etc.)
- **Save before referencing**: Always `save_asset()` the function before creating a `MaterialExpressionMaterialFunctionCall` to it
