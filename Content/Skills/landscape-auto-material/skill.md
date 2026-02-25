---
name: landscape-auto-material
display_name: Landscape Auto-Material System
description: >
  Create production-quality landscape materials using the master material +
  material function + material instance paradigm. Covers Runtime Virtual
  Textures, auto-layering (slope/altitude/distance blending), layer functions,
  and configuring biomes through material instances.
vibeue_classes:
  - MaterialService
  - MaterialNodeService
  - LandscapeMaterialService
  - RuntimeVirtualTextureService
unreal_classes:
  - MaterialExpressionMaterialFunctionCall
  - MaterialExpressionRuntimeVirtualTextureOutput
  - MaterialExpressionRuntimeVirtualTextureSample
  - RuntimeVirtualTexture
  - RuntimeVirtualTextureVolume
  - MaterialFunction
  - MaterialInstanceConstant
  - LandscapeLayerInfoObject
  - MaterialExpressionFunctionInput
  - MaterialExpressionFunctionOutput
keywords:
  - auto material
  - auto layer
  - master material
  - material function
  - material instance
  - runtime virtual texture
  - RVT
  - altitude blend
  - slope blend
  - distance blend
  - biome
  - landscape material
  - layer function
  - Real_Landscape
  - parametric material
  - function input
  - function output
  - bulk parameters
---

# Landscape Auto-Material System Skill

## When to Use This Skill

Use this skill when you need **production-quality** landscape materials with:
- A master material → material instance workflow (artists change parameters, not graphs)
- Automatic layer blending by slope, altitude, or distance
- Material functions for reusable logic (layer sampling, color correction, etc.)
- Runtime Virtual Textures for scalable landscape rendering
- Biome configuration through material instances (swap textures/thresholds, same master)

For **simple prototyping** with 2-5 painted layers, use `landscape-materials` instead:
```python
manage_skills(action="load", skill_name="landscape-materials")
```

## Architecture Overview

### The Master Material Paradigm

```
Master Material (M_Landscape_Master)
 ├── Material Functions (reusable logic blocks)
 │   ├── MF_AutoLayer     ── auto-selects layers by altitude/slope
 │   ├── MF_Slope_Blend   ── slope mask (0=flat, 1=steep)
 │   ├── MF_Altitude_Blend ── height mask (0=below, 1=above)
 │   ├── MF_Layer_Grass    ── per-layer texture sampling
 │   ├── MF_Layer_Rock     ── per-layer texture sampling
 │   ├── MF_RVT            ── Runtime Virtual Texture output
 │   └── MF_Distance_Blend ── LOD transitions
 ├── Exposed Parameters (50+ scalar/vector/texture/switch)
 └── Material Outputs (BaseColor, Normal, Roughness, WPO, RVT)

Material Instance (MI_Landscape_Biome_01)
 ├── Inherits master material graph
 ├── Overrides parameters only (no graph editing)
 └── Different biomes = different instances of same master
```

**Why this is better than manual graph building:**
- **Reusability**: Material functions used across multiple masters
- **Artist-friendly**: Biome artists edit parameters in instances, never touch the graph
- **Performance**: Compiled once in master, instances are cheap
- **Maintainability**: Fix a function → all materials using it update

## Critical Rules

### Four Services Work Together

| Service | Role |
|---------|------|
| **MaterialNodeService** | Material function creation/introspection, expression graphs |
| **MaterialService** | Material/instance creation, properties, bulk parameters |
| **LandscapeMaterialService** | `CreateAutoMaterial`, `FindLandscapeTextures`, layer infos |
| **RuntimeVirtualTextureService** | RVT assets, volumes, landscape assignment |

### ⚠️ compile_material Is Slow — Use Separate Code Blocks

Master materials with many function calls are **very slow** to compile. NEVER create + compile in the same block.

Split into steps:
1. **Block 1**: Create material, add functions, connect graph, save
2. **Block 2**: `compile_material()` alone (may take minutes)
3. **Block 3**: Create instances, set parameters, assign to landscape

### ⚠️ Enable Virtual Texturing on Material

If using RVT, you MUST set `bUsedWithVirtualTexturing = true` on the master material:
```python
unreal.MaterialService.set_material_property(mat_path, "bUsedWithVirtualTexturing", "true")
```

### ⚠️ Material Functions Must Be Saved Before Use

After creating a material function and adding inputs/outputs, **save it** before calling it from a material:
```python
unreal.EditorAssetLibrary.save_asset(func_path)
```

### ⚠️ Function Inputs/Outputs Need Sort Priority

Set `SortPriority` to control pin ordering. Lower values appear first:
```python
unreal.MaterialNodeService.add_function_input(func_path, "BaseColor", "Vector3", 0)
unreal.MaterialNodeService.add_function_input(func_path, "Normal", "Vector3", 1)
unreal.MaterialNodeService.add_function_input(func_path, "Roughness", "Scalar", 2)
```

---

## Real_Landscape Reference Architecture

The Real_Landscape content pack demonstrates this paradigm with:

```
M_Landscape_Master_01 (Master Material)
├── MF_AutoLayer_01 ──── Drives automatic layer selection
│   ├── MF_Altitude_Blend_01 ── Height-based layer transitions
│   └── MF_Slope_Blend_01 ──── Slope-based layer transitions
├── MF_Layer_Grass ──────── Per-layer texture sampling
├── MF_Layer_Rock ───────── Per-layer texture sampling
├── MF_Layer_Snow ───────── Per-layer texture sampling
├── MF_Layer_Dirt / Forest / Beach / Desert / Grass_Dry / Rock_Desert
├── MF_RVT_01 ──────────── Runtime Virtual Texture output
├── MF_Distance_Blends_01 ─ LOD transitions
├── MF_Distance_Fades_01 ── Far-distance fading
├── MF_BumpOffset_WorldAlignedNormal_01 ── Parallax
├── MF_Color_Correction_01 ── Color grading
├── MF_Color_Variations_01 ── Per-instance color variation
├── MF_Normal_Correction_01 ── Normal map fixes
├── MF_PBR_Conversion_01 ──── Roughness/metallic conversion
├── MF_Texture_Scale_01 ───── UV tiling control
├── MF_Displacement_01 ────── World position offset
└── MF_Wind_System_01 ────── Foliage wind animation
```

**Biome instances** override textures and blend thresholds:
- `MI_Landscape_Default_01` — grasslands with moderate slopes
- `MI_Landscape_Meadow_Island_01` — island variant with beach/sand
- `MI_Landscape_Meadow_Mountain_01` — alpine with more snow/rock

**Texture naming convention:**
| Suffix | Meaning | Example |
|--------|---------|---------|
| `_BC` | Base Color / Albedo | `T_Ground_Grass_01_BC` |
| `_N` | Normal Map | `T_Ground_Grass_01_N` |
| `_H` | Height Map | `T_Ground_Grass_01_H` |
| `_RMA` | Roughness-Metallic-AO packed | `T_Nature_Default_RMA_01` |

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
    True,  # bExposeToLibrary
    ["Landscape"]  # LibraryCategories
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
    "/Game/Real_Landscape/Core/Materials/Functions/MF_AutoLayer_01")

print(f"Description: {info.description}")
print(f"Expression count: {info.expression_count}")
print(f"Exposed to library: {info.expose_to_library}")

for inp in info.inputs:
    print(f"  Input: {inp.name} ({inp.input_type_name}) - {inp.description}")

for out in info.outputs:
    print(f"  Output: {out.name} ({out.input_type_name}) - {out.description}")

# Export full function graph as JSON
graph_json = unreal.MaterialNodeService.export_function_graph(
    "/Game/Real_Landscape/Core/Materials/Functions/MF_AutoLayer_01")
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

---

## Material Function Patterns

### MF_Slope_Blend — Slope Detection

Generates a 0-1 mask from the landscape surface normal angle:
- **Input**: World Normal (Vector3), Slope Threshold (Scalar), Blend Width (Scalar)
- **Output**: Slope Mask (Scalar) — 0 = flat, 1 = steep
- **Internal**: `dot(WorldNormal, UpVector)` → remap with threshold/width

### MF_Altitude_Blend — Height Detection

Generates a 0-1 mask from world position Z:
- **Input**: World Position (Vector3), Height Threshold (Scalar), Blend Height (Scalar)
- **Output**: Height Mask (Scalar) — 0 = below threshold, 1 = above
- **Internal**: `WorldPosition.Z` → smoothstep with threshold/blend

### MF_Layer_* — Per-Layer Texture Sampling

Each layer function encapsulates sampling for one terrain type:
- **Input**: BaseColor Texture (Texture2D), Normal Texture (Texture2D), UV Tiling (Scalar)
- **Output**: Sampled Color (Vector3), Sampled Normal (Vector3), Sampled Roughness (Scalar)
- **Internal**: TextureSample + UV scaling + optional detail blending

### MF_RVT — Runtime Virtual Texture Output

Wraps layer blend results into RVT output pins:
- **Input**: BaseColor (Vector3), Normal (Vector3), Roughness (Scalar)
- **Output**: (connects to `MaterialExpressionRuntimeVirtualTextureOutput` internally)

---

## Material Function Input Types

| Type | EFunctionInputType | Description |
|------|---------------------|-------------|
| `Scalar` | 0 | Single float value |
| `Vector2` | 1 | 2D vector (UV coords) |
| `Vector3` | 2 | 3D vector (color, normal, position) |
| `Vector4` | 3 | 4D vector (RGBA) |
| `Texture2D` | 4 | 2D texture reference |
| `TextureCube` | 5 | Cubemap texture |
| `TextureExternal` | 6 | External texture |
| `VolumeTexture` | 7 | 3D texture |
| `StaticBool` | 8 | Static switch (compile-time) |
| `MaterialAttributes` | 9 | Full material attributes struct |

---

## Common Mistakes

### 1. Forgetting `bUsedWithVirtualTexturing`
Material has RVT output node but rendering fails → set the property before compiling.

### 2. No RVT Volume Actor
Material outputs to RVT but nothing reads it → create `RuntimeVirtualTextureVolume` covering the landscape.

### 3. RVT MaterialType Mismatch
Volume expects `BaseColor_Normal_Roughness` but material only outputs `BaseColor` → types must match.

### 4. Not Saving Functions Before Referencing
Create function → immediately create function call → fails because function not saved. Always `save_asset()` the function first.

### 5. Missing `bExposeToLibrary`
Material function created but doesn't appear in the material editor's function library → set `bExposeToLibrary=True` and add `LibraryCategories`.

### 6. Compiling in Same Block as Creation
Master materials with many function calls take minutes to compile. Putting create + compile in one code block causes timeout.

### 7. Wrong Bulk Parameter Types
`set_instance_parameters_bulk` requires exact type strings: `"Scalar"`, `"Vector"`, `"Texture"`, `"StaticSwitch"`. Typos silently skip parameters.

### 8. Static Switch Parameters Need UpdateStaticPermutation
Static switches are compile-time. After setting via bulk set, the instance needs a static permutation update (handled internally by `set_instance_parameters_bulk`).

---

## Related Skills

| Task | Skills to Load |
|------|---------------|
| Sculpt terrain only | `landscape` |
| Simple painted material (2-5 layers) | `landscape` + `landscape-materials` |
| Auto-blending material (production) | `landscape` + `landscape-auto-material` |
| Material instances/biome configuration | `landscape-auto-material` |
| Material functions (non-landscape) | `materials` |
| Full pipeline (terrain + auto-material + RVT) | `landscape` + `landscape-auto-material` |
