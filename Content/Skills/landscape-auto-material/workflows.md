---
name: workflows
description: Step-by-step recipes for building landscape auto-materials, biome instances, material functions, and Runtime Virtual Textures.
---

## Workflows

### Workflow A: Create Auto-Material with Texture Discovery

Use `FindLandscapeTextures` to scan a directory and `CreateAutoMaterial` to build the material automatically:

**Block 1 — Discover textures:**
```python
import unreal

# Find landscape texture sets in a directory
textures = unreal.LandscapeMaterialService.find_landscape_textures("/Game/Textures/Landscape")

for tex_set in textures:
    print(f"{tex_set.terrain_type}: albedo={tex_set.albedo_path}, normal={tex_set.normal_path}")
```

**Block 2 — Build auto-material:**
```python
import unreal

# Configure layers from discovered textures
layers = [
    unreal.LandscapeAutoLayerConfig(
        layer_name="Grass",
        diffuse_texture_path="/Game/Textures/T_Ground_Grass_01_BC",
        normal_texture_path="/Game/Textures/T_Ground_Grass_01_N",
        roughness_texture_path="",
        tiling_scale=0.01,
        role="base"  # base layer (painted everywhere by default)
    ),
    unreal.LandscapeAutoLayerConfig(
        layer_name="Rock",
        diffuse_texture_path="/Game/Textures/T_Ground_Rock_01_BC",
        normal_texture_path="/Game/Textures/T_Ground_Rock_01_N",
        roughness_texture_path="",
        tiling_scale=0.01,
        role="slope"  # auto-blend on steep surfaces
    ),
    unreal.LandscapeAutoLayerConfig(
        layer_name="Snow",
        diffuse_texture_path="/Game/Textures/T_Ground_Snow_01_BC",
        normal_texture_path="/Game/Textures/T_Ground_Snow_01_N",
        roughness_texture_path="",
        tiling_scale=0.01,
        role="height"  # auto-blend at high altitude
    ),
]

result = unreal.LandscapeMaterialService.create_auto_material(
    "M_Terrain_Auto",
    "/Game/Materials",
    layers,
    "MyLandscape"  # optional: assign to landscape
)
print(f"Material: {result.material_asset_path}")
print(f"Layer infos: {result.layer_info_paths}")
```

**Block 3 — Compile (slow!):**
```python
import unreal
unreal.MaterialService.compile_material("/Game/Materials/M_Terrain_Auto")
```

### Workflow B: Create Material Instance for a New Biome

```python
import unreal

# Create instance from master material
inst = unreal.MaterialService.create_material_instance(
    "MI_Landscape_Tropical",
    "/Game/Materials",
    "/Game/Materials/M_Landscape_Master"
)
inst_path = inst.asset_path

# Set all biome parameters at once using bulk set
count = unreal.MaterialService.set_instance_parameters_bulk(
    inst_path,
    # Names array
    ["GrassTexture", "RockTexture", "SandTexture",
     "SlopeThreshold", "SlopeBlendWidth", "AltitudeThreshold",
     "EnableSnow"],
    # Types array
    ["Texture", "Texture", "Texture",
     "Scalar", "Scalar", "Scalar",
     "StaticSwitch"],
    # Values array
    ["/Game/Textures/T_Tropical_Grass_BC",
     "/Game/Textures/T_Tropical_Rock_BC",
     "/Game/Textures/T_Tropical_Sand_BC",
     "0.7", "0.15", "5000.0",
     "false"]
)
print(f"Set {count} parameters on {inst_path}")
```

### Workflow C: Create a Material Function

**Block 1 — Create function with inputs/outputs:**
```python
import unreal

# Create the function asset
func = unreal.MaterialNodeService.create_material_function(
    "MF_Layer_Tropical",
    "/Game/Materials/Functions",
    "Samples and blends tropical layer textures",
    True  # bExposeToLibrary
)
func_path = func.asset_path

# Add inputs
bc_input = unreal.MaterialNodeService.add_function_input(
    func_path, "BaseColorTexture", "Texture2D", 0, "Diffuse texture for this layer")
n_input = unreal.MaterialNodeService.add_function_input(
    func_path, "NormalTexture", "Texture2D", 1, "Normal map texture")
tiling_input = unreal.MaterialNodeService.add_function_input(
    func_path, "Tiling", "Scalar", 2, "UV tiling scale")

# Add outputs
bc_output = unreal.MaterialNodeService.add_function_output(
    func_path, "BlendedColor", 0, "Sampled and blended base color")
n_output = unreal.MaterialNodeService.add_function_output(
    func_path, "BlendedNormal", 1, "Sampled normal map")

# Save before adding internal nodes
unreal.EditorAssetLibrary.save_asset(func_path)
print(f"Created function: {func_path}")
```

**Block 2 — Build internal graph:**
```python
import unreal

func_path = "/Game/Materials/Functions/MF_Layer_Tropical"

# Create texture sampler inside the function
sampler_id = unreal.MaterialNodeService.create_function_expression(
    func_path, "MaterialExpressionTextureSample", -300, 0)

# Connect input to sampler, output from sampler
# (Use expression IDs returned from create calls)
unreal.MaterialNodeService.connect_function_expressions(
    func_path, bc_input, "", sampler_id, "Tex")

unreal.MaterialNodeService.connect_function_expressions(
    func_path, sampler_id, "RGB", bc_output, "")

unreal.EditorAssetLibrary.save_asset(func_path)
```

**Block 3 — Use function in a material:**
```python
import unreal

mat_path = "/Game/Materials/M_Landscape_Master"
func_path = "/Game/Materials/Functions/MF_Layer_Tropical"

# Create function call node in the material
call_id = unreal.MaterialNodeService.create_expression(
    mat_path, "MaterialExpressionMaterialFunctionCall", -600, 0)

# Set the function reference
unreal.MaterialNodeService.set_expression_property(
    mat_path, call_id, "MaterialFunction",
    f"MaterialFunction'{func_path}.MF_Layer_Tropical'")

# Connect function output to material
unreal.MaterialNodeService.connect_to_output(
    mat_path, call_id, "BlendedColor", "BaseColor")

unreal.MaterialService.compile_material(mat_path)
```

### Workflow D: Inspect and Export Material Functions

```python
import unreal

# Get function interface information
info = unreal.MaterialNodeService.get_function_info(
    "/Game/Materials/Functions/MF_AutoLayer")

print(f"Description: {info.description}")
print(f"Expression count: {info.expression_count}")
print(f"Exposed to library: {info.expose_to_library}")

for inp in info.inputs:
    print(f"  Input: {inp.name} ({inp.input_type_name}) - {inp.description}")

for out in info.outputs:
    print(f"  Output: {out.name} ({out.input_type_name}) - {out.description}")

# Export full function graph as JSON
graph_json = unreal.MaterialNodeService.export_function_graph(
    "/Game/Materials/Functions/MF_AutoLayer")
print(graph_json)
```

### Workflow E: Setup Runtime Virtual Textures

**Block 1 — Create RVT asset:**
```python
import unreal

# Create RVT asset
result = unreal.RuntimeVirtualTextureService.create_runtime_virtual_texture(
    "RVT_Landscape_01",
    "/Game/VirtualTextures",
    "BaseColor_Normal_Roughness",  # MaterialType
    256,   # TileCount
    256,   # TileSize
    4,     # TileBorderSize
    False, # bContinuousUpdate
    False  # bSinglePhysicalSpace
)
print(f"RVT: {result.asset_path}")
```

**Block 2 — Create volume and assign:**
```python
import unreal

rvt_path = "/Game/VirtualTextures/RVT_Landscape_01"

# Create volume actor covering the landscape
vol = unreal.RuntimeVirtualTextureService.create_rvt_volume(
    "MyLandscape",  # landscape name or label
    rvt_path,
    "RVT_Volume_MyLandscape"  # volume actor name
)
print(f"Volume: {vol.volume_label}")

# Assign RVT to landscape slot 0
unreal.RuntimeVirtualTextureService.assign_rvt_to_landscape(
    "MyLandscape", rvt_path, 0)
```

**Block 3 — Add RVT output to material:**
```python
import unreal

mat_path = "/Game/Materials/M_Landscape_Master"

# Enable virtual texturing on the material
unreal.MaterialService.set_material_property(mat_path, "bUsedWithVirtualTexturing", "true")

# Create RVT output node
rvt_out = unreal.MaterialNodeService.create_expression(
    mat_path, "MaterialExpressionRuntimeVirtualTextureOutput", 400, 0)

# Connect BaseColor to RVT output
# (Assumes blend_id is the output of your layer blending chain)
unreal.MaterialNodeService.connect_expressions(
    mat_path, blend_id, "RGB", rvt_out, "BaseColor")
unreal.MaterialNodeService.connect_expressions(
    mat_path, normal_id, "RGB", rvt_out, "Normal")
unreal.MaterialNodeService.connect_expressions(
    mat_path, roughness_id, "", rvt_out, "Roughness")

unreal.MaterialService.compile_material(mat_path)
```
