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
    - production landscape workflow
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

## Reference Architecture Example

A production auto-material setup typically looks like this:

```
M_Landscape_Master (Master Material)
├── MF_AutoLayer ─────── Drives automatic layer selection
│   ├── MF_Altitude_Blend ── Height-based layer transitions
│   └── MF_Slope_Blend ───── Slope-based layer transitions
├── MF_Layer_Grass ──────── Per-layer texture sampling
├── MF_Layer_Rock ───────── Per-layer texture sampling
├── MF_Layer_Snow ───────── Per-layer texture sampling
├── MF_Layer_Dirt / Forest / Beach / Desert / Grass_Dry / Rock_Desert
├── MF_RVT ─────────────── Runtime Virtual Texture output
├── MF_Distance_Blends ─── LOD transitions
├── MF_Distance_Fades ──── Far-distance fading
├── MF_BumpOffset_WorldAlignedNormal ── Parallax
├── MF_Color_Correction ── Color grading
├── MF_Color_Variations ── Per-instance color variation
├── MF_Normal_Correction ─ Normal map fixes
├── MF_PBR_Conversion ──── Roughness/metallic conversion
├── MF_Texture_Scale ───── UV tiling control
├── MF_Displacement ────── World position offset
└── MF_Wind_System ─────── Foliage wind animation
```

**Biome instances** override textures and blend thresholds:
- `MI_Landscape_Default_01` — grasslands with moderate slopes
- `MI_Landscape_Island_01` — island variant with beach/sand
- `MI_Landscape_Mountain_01` — alpine with more snow/rock

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
Material function created but doesn't appear in the material editor's function library → set `bExposeToLibrary=True`.

### 6. Compiling in Same Block as Creation
Master materials with many function calls take minutes to compile. Putting create + compile in one code block causes timeout.

### 7. Wrong Bulk Parameter Types
`set_instance_parameters_bulk` requires exact type strings: `"Scalar"`, `"Vector"`, `"Texture"`, `"StaticSwitch"`. Typos silently skip parameters.

### 8. Static Switch Parameters Need UpdateStaticPermutation
Static switches are compile-time. After setting via bulk set, the instance needs a static permutation update (handled internally by `set_instance_parameters_bulk`).

### 9. Auto Layer Has Zero Weights
Landscape looks unpainted or auto-blend never appears when all layer weights are 0. Verify with `get_weights_in_region` and fill/paint the base auto layer at least once.

### 10. Layer Info/Material Family Mismatch
If a landscape is switched to a different master/instance family, existing Layer Info assets may not match expected layer names/workflow. Recreate layer infos from the active material's layer list and reassign them.

### 11. RVT Volume Missing or Mis-sized
RVT asset assignment alone is not enough. Ensure each landscape has an RVT volume covering its bounds; recreate volume per landscape after major transform/scale changes.

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

---

## Auto-Layer Architecture Details

### Function Chain

```
MF_AutoLayer_01 (orchestrator)
│
├── Reads: World Position (from VertexNormalWS / AbsoluteWorldPosition)
├── Reads: World Normal (from VertexNormalWS)
│
├── Calls: MF_Altitude_Blend_01
│   ├── Input: WorldPosition.Z
│   ├── Parameter: Height Threshold (e.g., 5000 units)
│   ├── Parameter: Blend Height (transition width, e.g., 500 units)
│   └── Output: Height Mask (0.0 = below, 1.0 = above)
│
├── Calls: MF_Slope_Blend_01
│   ├── Input: World Normal
│   ├── Parameter: Slope Threshold (0.0-1.0, where 0.7 ≈ 45°)
│   ├── Parameter: Slope Blend Width (e.g., 0.15)
│   └── Output: Slope Mask (0.0 = flat, 1.0 = steep)
│
├── Combines masks:
│   ├── SlopeMask → Rock layer weight
│   ├── HeightMask → Snow layer weight
│   ├── (1 - SlopeMask) * (1 - HeightMask) → Base layer weight (Grass)
│   └── Optional: SlopeMask * HeightMask → Alpine Rock variant
│
└── Per-layer sampling:
    ├── MF_Layer_Grass(weight=base) → Sampled BaseColor, Normal, Roughness
    ├── MF_Layer_Rock(weight=slope) → Sampled BaseColor, Normal, Roughness
    ├── MF_Layer_Snow(weight=height) → Sampled BaseColor, Normal, Roughness
    └── Lerp/Blend all layers by weights → Final output
```

### How Masks Combine

Auto-layer uses **multiplicative mask composition**:

```
base_weight  = (1.0 - slope_mask) * (1.0 - height_mask)
slope_weight = slope_mask * (1.0 - height_mask)  // Rock on slopes but not peaks
height_weight = height_mask                        // Snow on peaks regardless of slope
```

This ensures weights always sum to ~1.0 without explicit normalization.

### Height Blend vs. Weight Blend

- **Auto-layer weights** (from slope/altitude) use `LB_HeightBlend` for smooth transitions
- **Painted layers** use `LB_WeightBlend` for artist control
- When both exist on the same landscape, painted layers **override** auto-layer in painted regions

### Creating the Slope Blend Function

```python
import unreal

func = unreal.MaterialNodeService.create_material_function(
    "MF_Slope_Blend", "/Game/Materials/Functions",
    "Generates a slope mask from world normal", True, ["Landscape"])

unreal.MaterialNodeService.add_function_input(
    func.asset_path, "SlopeThreshold", "Scalar", 0, "Dot product threshold (0.7 ≈ 45°)")
unreal.MaterialNodeService.add_function_input(
    func.asset_path, "BlendWidth", "Scalar", 1, "Transition smoothness")

unreal.MaterialNodeService.add_function_output(
    func.asset_path, "SlopeMask", 0, "0=flat, 1=steep")

unreal.EditorAssetLibrary.save_asset(func.asset_path)
```

### Creating the Altitude Blend Function

```python
import unreal

func = unreal.MaterialNodeService.create_material_function(
    "MF_Altitude_Blend", "/Game/Materials/Functions",
    "Generates a height mask from world position Z", True, ["Landscape"])

unreal.MaterialNodeService.add_function_input(
    func.asset_path, "HeightThreshold", "Scalar", 0, "World Z height to start blending")
unreal.MaterialNodeService.add_function_input(
    func.asset_path, "BlendHeight", "Scalar", 1, "Height range for transition")

unreal.MaterialNodeService.add_function_output(
    func.asset_path, "HeightMask", 0, "0=below threshold, 1=above")

unreal.EditorAssetLibrary.save_asset(func.asset_path)
```

### Extending the Auto-Layer System

To add a new terrain condition (e.g., moisture/wetness):
1. Create a new mask function `MF_Moisture_Blend`
2. Add moisture mask to the `MF_AutoLayer` combiner
3. Multiply into layer weights alongside slope/height
4. Expose threshold parameters for instance override

---

## Biome Configuration Guide

A **biome** is a specific landscape look defined entirely through a **material instance** of the master material. The master graph stays unchanged — biomes differ only in parameter values.

### Biome Comparison Table

| Parameter | Default | Island Variant | Mountain Variant |
|-----------|---------|--------------|-----------------|
| SlopeThreshold | 0.7 | 0.75 | 0.6 |
| AltitudeThreshold | 5000 | 500 | 8000 |
| EnableSnow | true | false | true |
| Layer01 (base) | Grass | Tropical Grass | Alpine Grass |
| Layer02 (slope) | Rock | Sandstone | Granite |
| Layer03 (height) | Snow | Beach Sand | Snow |
| ColorSaturation | 1.0 | 1.2 | 0.9 |
| UVTiling | 0.01 | 0.01 | 0.008 |

### Configure Feature Toggles via Static Switches

```python
count = unreal.MaterialService.set_instance_parameters_bulk(
    inst_path,
    ["EnableSnow", "EnableDisplacement", "EnableRVT", "EnableDistanceFades"],
    ["StaticSwitch", "StaticSwitch", "StaticSwitch", "StaticSwitch"],
    ["false", "true", "true", "true"]
)
```

### Biome Naming Convention

```
MI_Landscape_<Region>_<Variant>_##
```
Examples:
- `MI_Landscape_Default_01` — generic grassland
- `MI_Landscape_Tropical_Island_01` — tropical island variant
- `MI_Landscape_Alpine_Valley_01` — alpine valley
- `MI_Landscape_Desert_Canyon_01` — desert canyon

---

## Layer Function Template

A layer function encapsulates all texture sampling logic for a single terrain type. This makes per-terrain materials modular and reusable.

### Standard Interface

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

### Internal Graph Pattern

```
LandscapeLayerCoords (UV tiling)
    ↓
TextureSample (BaseColor) → Output: BaseColor
TextureSample (Normal)    → Output: Normal
TextureSample (Roughness) → Output: Roughness
    ↓ (optional)
DetailTextureSample → Lerp with base by distance → Final outputs
```

### Layer Function Catalog

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

### Layer Function Tips

- **Keep functions focused**: One terrain type per function, one concern per function
- **Use consistent pin naming**: `BaseColor`, `Normal`, `Roughness` for all output functions
- **Sort priorities matter**: Keep consistent across all layer functions (0=BC, 1=Normal, 2=Roughness, 3=UV)
- **Save before referencing**: Always `save_asset()` the function before creating a `MaterialExpressionMaterialFunctionCall` to it

---

## Parameter Reference

### Layer Texture Parameters (per layer)

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `Layer##_BaseColor` | Texture | Albedo/diffuse texture for layer | None |
| `Layer##_Normal` | Texture | Normal map for layer | FlatNormal |
| `Layer##_Roughness` | Texture | Roughness (or RMA packed) | Gray |
| `Layer##_Height` | Texture | Height map for height-blending | White |
| `Layer##_UVTiling` | Scalar | UV tiling scale | 0.01 |
| `Layer##_DetailBaseColor` | Texture | Close-up detail texture | None |
| `Layer##_DetailScale` | Scalar | Detail texture tiling multiplier | 5.0 |
| `Layer##_RoughnessMin` | Scalar | Roughness remap minimum | 0.0 |
| `Layer##_RoughnessMax` | Scalar | Roughness remap maximum | 1.0 |

*`##` = layer index: 01, 02, 03, etc.*

### Auto-Blend Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `SlopeThreshold` | Scalar | Dot product threshold for slope detection (0.7 ≈ 45°) | 0.7 |
| `SlopeBlendWidth` | Scalar | Slope transition smoothness | 0.15 |
| `AltitudeThreshold` | Scalar | World Z height where altitude blend begins | 5000.0 |
| `AltitudeBlendHeight` | Scalar | Height range for altitude transition | 500.0 |
| `HeightBlendSharpness` | Scalar | Height-based layer transition sharpness | 10.0 |

### Distance & LOD Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `NearDistance` | Scalar | Distance for full-detail rendering | 1000.0 |
| `FarDistance` | Scalar | Distance for simplified rendering | 10000.0 |
| `FadeDistance` | Scalar | Distance at which material fades completely | 50000.0 |
| `DetailBlendDistance` | Scalar | Distance for detail texture fade-in | 500.0 |

### Color Correction Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `ColorSaturation` | Scalar | Overall color saturation | 1.0 |
| `ColorBrightness` | Scalar | Overall brightness multiplier | 1.0 |
| `ColorContrast` | Scalar | Color contrast adjustment | 1.0 |
| `ColorTint` | Vector | Per-biome color tint (RGB) | (1,1,1,1) |
| `NormalIntensity` | Scalar | Normal map strength multiplier | 1.0 |
| `RoughnessScale` | Scalar | Global roughness scaling | 1.0 |

### Feature Toggles (Static Switches)

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `EnableSnow` | StaticSwitch | Enable snow layer (altitude blend) | true |
| `EnableDisplacement` | StaticSwitch | Enable world position offset | false |
| `EnableRVT` | StaticSwitch | Enable Runtime Virtual Texture output | true |
| `EnableDistanceFades` | StaticSwitch | Enable distance-based LOD | true |
| `EnableDetailTextures` | StaticSwitch | Enable close-up detail textures | true |
| `EnableColorCorrection` | StaticSwitch | Enable per-biome color grading | true |
| `EnableBumpOffset` | StaticSwitch | Enable parallax/bump offset | false |
| `EnableWindSystem` | StaticSwitch | Enable vegetation wind animation | false |

### Displacement / WPO Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `DisplacementScale` | Scalar | World position offset strength | 10.0 |
| `DisplacementOffset` | Scalar | Displacement center offset | 0.0 |
| `TessellationMultiplier` | Scalar | Tessellation density (if supported) | 1.0 |

### RVT Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `RVT_VirtualTexture` | Texture | Runtime Virtual Texture asset reference | None |
| `RVT_WorldHeight` | Scalar | Reference world height for RVT | 0.0 |

### FindLandscapeTextures Suffix Matching

The `FindLandscapeTextures` tool recognizes these suffixes when scanning directories:

**Albedo/BaseColor:** `_Albedo`, `_Diffuse`, `_BaseColor`, `_D`, `_Color`, `_BC`, `_Base`

**Normal:** `_Normal`, `_N`, `_NormalMap`, `_Norm`

**Roughness:** `_Roughness`, `_R`, `_Rough`, `_RMA`, `_ORM`, `_ARM`

**Terrain type inference from path keywords:**
Grass, Rock, Snow, Dirt, Sand, Forest, Mud, Clay, Gravel, Moss, Bark, Ground, Soil, Stone, Pebble, Mountain, Alpine, Desert, Beach, Cliff, Tundra, Swamp, Wetland, Ice

---

## RVT Setup Guide

### RVT Material Types

| Type | Channels | Use Case |
|------|----------|----------|
| `BaseColor` | RGB | Color-only caching (simplest) |
| `BaseColor_Normal_Roughness` | RGB + Normal + Roughness | Standard landscape (most common) |
| `BaseColor_Normal_Specular` | RGB + Normal + Specular | For specular-heavy materials |
| `WorldHeight` | Height only | For distance-based effects, atmosphere |

**The RVT material type must match what your material actually outputs.**

### RVT Sizing Guidelines

| Landscape Size | TileCount | TileSize | Total Resolution |
|----------------|-----------|----------|-----------------|
| Small (1-4 km²) | 128 | 256 | 32K × 32K |
| Medium (4-16 km²) | 256 | 256 | 64K × 64K |
| Large (16+ km²) | 512 | 256 | 128K × 128K |

**Notes:**
- Higher tile count = more memory but better streaming
- `bContinuousUpdate = true` re-renders every frame (expensive, only for dynamic materials)
- `bSinglePhysicalSpace = true` disables streaming (entire VT in memory, fast but memory-heavy)

### Inspecting Existing RVTs

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

### Common RVT Issues

| Issue | Cause | Fix |
|-------|-------|-----|
| RVT Shows Black | Missing `bUsedWithVirtualTexturing = true` | Set property on material |
| RVT Shows Black | RVT output node not connected | Connect matching pins |
| RVT Shows Black | No RVT Volume actor in level | Create volume via `create_rvt_volume` |
| RVT Shows Blurry | TileCount/TileSize too low | Increase values |
| RVT Shows Blurry | Volume bounds don't match landscape | Recreate volume |
| Performance Regression | `bContinuousUpdate` enabled unnecessarily | Disable unless material changes per-frame |
| Performance Regression | Volume much larger than landscape | Resize to match |
