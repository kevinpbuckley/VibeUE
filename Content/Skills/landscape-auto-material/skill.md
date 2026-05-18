---
name: landscape-auto-material
display_name: Landscape Auto-Material System
description: Create production-quality landscape materials using the master material + material function + material instance paradigm. Covers Runtime Virtual Textures, auto-layering (slope/altitude/distance blending), layer functions, and configuring biomes through material instances.
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

### 🚨 Inspect Before Modifying Existing Materials or Functions

Before modifying an **existing** master material or material function, **MUST** export and review its current state:

```python
import unreal, json

# For materials:
graph = json.loads(unreal.MaterialNodeService.export_material_graph("/Game/Materials/M_Landscape_Master"))
print(f"Expressions: {len(graph['expressions'])}, Connections: {len(graph['connections'])}")
for expr in graph['expressions']:
    name = expr.get('parameter_name') or expr.get('function_path') or expr.get('class')
    print(f"  [{expr['id']}] {expr['class']} - {name}")

# For material functions:
func_info = unreal.MaterialNodeService.get_function_info("/Game/Functions/MF_AutoLayer")
func_graph = json.loads(unreal.MaterialNodeService.export_function_graph("/Game/Functions/MF_AutoLayer"))
```

**Why:** Auto-material master graphs can have 50+ expressions and complex function call chains. Adding nodes without reviewing first creates duplicates, broken connections, and compilation failures. Always export → review → plan → modify.

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

## Sub-docs available

Load these via `manage_skills(action="load", skill_name="landscape-auto-material")` reads the index; for the deeper docs, open the sibling files directly:

| Sub-doc | What's inside |
|---------|---------------|
| `reference-example.md` | Concrete reference architecture: full master material function chain, biome instances, texture naming convention |
| `workflows.md` | Step-by-step recipes: auto-material creation, biome instances, building/inspecting material functions, RVT setup |
| `material-functions.md` | Patterns for common material functions (Slope_Blend, Altitude_Blend, Layer_*, RVT) + EFunctionInputType reference table |
| `architecture-details.md` | Deep dive on the auto-layer function chain, mask combination math, slope/altitude function creation, extension points |
| `biomes-and-templates.md` | Biome comparison table, static-switch feature toggles, naming conventions, standard layer function template (inputs/outputs/catalog) |
| `parameter-reference.md` | Full parameter catalog: layer textures, auto-blend, distance/LOD, color correction, feature toggles, displacement, RVT, FindLandscapeTextures suffix matching |
| `rvt-setup.md` | RVT material types, sizing guidelines, inspection code, common RVT issues with causes/fixes |

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
