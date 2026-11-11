---
name: landscape-auto-material
display_name: Landscape Auto-Material System
description: Create production-quality landscape materials with the master-material + material-function + material-instance paradigm, RVT, and auto-layering. Use when the user asks for an auto/procedural landscape material, slope/altitude/distance-based layer blending, Runtime Virtual Textures, biome configuration via instances, or layer material functions. For basic layer-blend materials load landscape-materials.
vibeue_classes:
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

For **simple prototyping** with 2-5 painted layers, load the **landscape-materials** skill instead (the engine's AgentSkillToolset `GetSkills` exposes it).

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

Graph export/introspection is **Epic's** now — `MaterialNodeService.export_material_graph`,
`get_function_info` and `export_function_graph` are gone. `MaterialToolset` reads both materials and
material functions through the same `MaterialOrFunction` parameter, so one pattern covers both
(`call_tool`; `refPath` is the full object path):

```
# Every expression in the graph (a material OR a material function):
call_tool MaterialToolset.GetExpressions   {"MaterialOrFunction": {"refPath": "/Game/Materials/M_Landscape_Master.M_Landscape_Master"}}
call_tool MaterialToolset.GetExpressions   {"MaterialOrFunction": {"refPath": "/Game/Functions/MF_AutoLayer.MF_AutoLayer"}}

# What feeds each expression, and what feeds the material outputs:
call_tool MaterialToolset.GetExpressionInputs {"MaterialOrFunction": {...}, "Expression": {"refPath": "<expression from GetExpressions>"}}
call_tool MaterialToolset.GetPropertyInput    {"Material": {...}, "MaterialProperty": "MP_BaseColor"}

# Any property of any expression (parameter names, defaults, function input type/sort priority):
call_tool ObjectToolset.GetProperties {"Object": {"refPath": "<expression>"}}
```

There is no single JSON blob any more: `GetExpressions` gives you the nodes and
`GetExpressionInputs` the wiring, one expression at a time — so walk it, and keep a note of what you
saw rather than re-reading the whole graph for every edit.

**Why:** Auto-material master graphs can have 50+ expressions and complex function call chains. Adding nodes without reviewing first creates duplicates, broken connections, and compilation failures. Always export → review → plan → modify.

### Two VibeUE Services + Epic's Material Toolsets Work Together

VibeUE keeps the landscape-specific halves; everything generic about materials is Epic's, reached
with `call_tool`. **`MaterialService` is deleted**, and `MaterialNodeService` is down to exactly two
tools.

| Where | Role |
|---------|------|
| **LandscapeMaterialService** (`execute_python`) | `create_auto_material`, `find_landscape_textures`, layer blend/weight/coords/sample nodes, layer infos, landscape assignment |
| **RuntimeVirtualTextureService** (`execute_python`) | RVT assets, volumes, landscape assignment |
| **MaterialNodeService** (`execute_python`) | exactly two tools left: `create_function_call` and `create_collection_parameter` |
| **`MaterialToolset`** (`call_tool`) | `CreateMaterial`, `CreateFunction`, `AddExpression`, `ConnectExpressions`, `ConnectToOutput`, `GetExpressions`, `GetExpressionInputs`, `LayoutExpressions`, `Recompile`, `GetStatistics` |
| **`MaterialInstanceToolset`** (`call_tool`) | `Create`, `SetParent`, `ListParameters`, `SetScalarParameter` / `SetVectorParameter` / `SetTextureParameter` / `SetStaticSwitchParameter` |
| **`ObjectToolset`** (`call_tool`) | `GetProperties` / `SetProperties` for **any** material or expression property |
| **`AssetToolset`** (`call_tool`) | `SaveAsset`, `Exists`, `Duplicate` |

### ⚠️ Recompiling Is Slow — Use Separate Code Blocks

Master materials with many function calls are **very slow** to compile. NEVER create + compile in the same block.

Split into steps:
1. **Block 1**: Create material, add functions, connect graph, save
2. **Block 2**: `call_tool MaterialToolset.Recompile {"MaterialOrFunction": {"refPath": "<mat>"}}`
   alone (may take minutes). For a material **function**, `Recompile` also recompiles every material
   that references it — so it is slower still.
3. **Block 3**: Create instances, set parameters, assign to landscape

### ⚠️ Enable Virtual Texturing on Material

If using RVT, you MUST set `bUsedWithVirtualTexturing = true` on the master material. Material
properties are Epic's `ObjectToolset` now (`MaterialService.set_property` is gone):

```
call_tool ObjectToolset.SetProperties {"Object": {"refPath": "/Game/Materials/M_Terrain.M_Terrain"},
                                       "Properties": {"bUsedWithVirtualTexturing": true}}
```

Set it **before** the recompile, then read it back with `ObjectToolset.GetProperties`.

### ⚠️ Material Functions Must Be Saved Before Use

After creating a material function and adding inputs/outputs, **save it** before calling it from a material:
```python
unreal.EditorAssetLibrary.save_asset(func_path)
```

### ⚠️ Function Inputs/Outputs Need Sort Priority

`MaterialNodeService.add_function_input` / `add_function_output` are gone. A function input is now an
expression you add to the function and then configure — two `call_tool` calls per pin:

```
call_tool MaterialToolset.AddExpression {"MaterialOrFunction": {"refPath": "<MF path>"},
          "ExpressionClass": "/Script/Engine.MaterialExpressionFunctionInput", "X": -600, "Y": 0}
call_tool ObjectToolset.SetProperties   {"Object": {"refPath": "<the expression returned above>"},
          "Properties": {"InputName": "BaseColor", "InputType": "FunctionInput_Vector3", "SortPriority": 0}}
```

`SortPriority` controls pin ordering — lower values appear first, so give `BaseColor` 0, `Normal` 1,
`Roughness` 2. Outputs are the same with `MaterialExpressionFunctionOutput` and `OutputName`.
`ListExpressionClasses` on the function finds the exact class path if the one above is rejected.

---

## Sub-docs available

Read these sibling files directly (the engine AgentSkillToolset `GetSkills` is the loader; this `SKILL.md` is the index, the deeper docs live alongside it):

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

### 7. Reaching for a Bulk Parameter Setter
`MaterialService.set_instance_parameters_bulk` is gone, and nothing batches instance parameters any
more: **you iterate**. One `call_tool` per parameter, each typed —
`MaterialInstanceToolset.SetScalarParameter` / `SetVectorParameter` / `SetTextureParameter` /
`SetStaticSwitchParameter` — so pick the call from the parameter's real type (`ListParameters` on the
parent tells you) and check each reply instead of trusting a count. A typo no longer "silently
skips": the wrong typed call fails on that one parameter, leaving the rest of the biome applied.

### 8. Static Switch Parameters Need a Recompile
Static switches are compile-time, and nothing updates the static permutation for you now. After
`MaterialInstanceToolset.SetStaticSwitchParameter`, `AssetToolset.SaveAsset` the instance and
`MaterialToolset.Recompile` it, then confirm with `MaterialInstanceToolset.GetStaticSwitchParameter`.

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

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [architecture-details](architecture-details.md) - Deep dive on the auto-layer function chain, mask combination math, slope/altitude function creation, and extension points.
- [biomes-and-templates](biomes-and-templates.md) - Biome configuration guide (comparison table, static-switch toggles, naming) and the standard layer function template (inputs, outputs, catalog).
- [material-functions](material-functions.md) - Patterns for common landscape material functions (Slope_Blend, Altitude_Blend, Layer_*, RVT) and the EFunctionInputType reference table.
- [parameter-reference](parameter-reference.md) - Full landscape master-material parameter catalog — layer textures, auto-blend, distance/LOD, color, feature toggles, displacement, RVT, and FindLandsc...
- [reference-example](reference-example.md) - Concrete reference architecture for a production landscape master material — full function chain, biome instances, and texture naming convention.
- [rvt-setup](rvt-setup.md) - Runtime Virtual Texture material types, sizing guidelines, inspection code, and common RVT issues with causes and fixes.
- [workflows](workflows.md) - Step-by-step recipes for building landscape auto-materials, biome instances, material functions, and Runtime Virtual Textures.
