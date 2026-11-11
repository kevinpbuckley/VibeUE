---
name: landscape-materials
display_name: Landscape Material System
description: Create landscape materials with layer blending, texture setup, and layer info objects (LandscapeMaterialService). Use when the user asks to build a landscape/terrain material, set up paint layers (grass/rock/dirt), create layer info objects, add a grass output, or assign a material to a landscape. For production master-material+RVT systems, load landscape-auto-material.
vibeue_classes:
  - LandscapeMaterialService
  - MaterialNodeService
engine_toolsets:
  - EditorToolset.MaterialToolset
  - EditorToolset.ObjectToolset
  - EditorToolset.AssetToolset
unreal_classes:
  - MaterialExpressionLandscapeLayerBlend
  - MaterialExpressionLandscapeLayerWeight
  - MaterialExpressionLandscapeLayerCoords
  - LandscapeLayerInfoObject
keywords:
  - landscape material
  - layer blend
  - terrain material
  - weight blend
  - layer info
  - terrain texture
  - height blend
  - slope blend
  - auto paint
  - height mask
  - slope mask
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "materials-and-shaders"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Landscape Material System Skill

## When to Use This Skill vs. landscape-auto-material

- **This skill** (`landscape-materials`): Simple materials with 2-5 layers using `LandscapeLayerBlend`. Good for prototyping.
- **`landscape-auto-material`**: Production materials using material functions, RVT, and material instances. Good for shipping quality. Use when you need the Real_Landscape paradigm with auto-layering, altitude/slope blending via material functions, and biome configuration through material instances.

If you need material functions, Runtime Virtual Textures, or the master-material + instance pattern, load the **landscape-auto-material** skill (the engine's AgentSkillToolset `GetSkills` exposes it).

## Critical Rules

### 🚨 Inspect Before Modifying Existing Landscape Materials

Before modifying an **existing** landscape material, you **MUST** export and review its current graph:

Both reads are Epic's now — `MaterialService.get_material_info` and
`MaterialNodeService.export_material_graph` are gone, and there is no single JSON graph dump. Walk it
with `call_tool` (`refPath` is the full object path, `/Game/Materials/M_Terrain.M_Terrain`):

```
# Step 1: the material's own properties (blend mode, shading model, two-sided, ...)
call_tool ObjectToolset.GetProperties {"Object": {"refPath": "/Game/Materials/M_Terrain.M_Terrain"}}

# Step 2: every expression that already exists — look for LandscapeLayerBlend / LayerCoords /
#         TextureSample before you add anything
call_tool MaterialToolset.GetExpressions {"MaterialOrFunction": {"refPath": "<material>"}}

# Step 3: the layers on a LayerBlend node, and any other expression field
call_tool ObjectToolset.GetProperties {"Object": {"refPath": "<the LayerBlend expression>"}}
#   ... or, VibeUE's landscape-aware read of the same node:
#   unreal.LandscapeMaterialService.get_layer_blend_info(mat_path, node_id)   # execute_python

# Step 4: what is wired where — per expression, and per material output
call_tool MaterialToolset.GetExpressionInputs {"MaterialOrFunction": {"refPath": "<material>"},
                                               "Expression": {"refPath": "<expression>"}}
call_tool MaterialToolset.GetPropertyInput    {"Material": {"refPath": "<material>"},
                                               "MaterialProperty": "MP_BaseColor"}
```

**Why:** Landscape materials often already have LayerBlend nodes, texture samplers, and connected outputs. Adding duplicate nodes without reviewing first creates orphaned expressions and broken connections. Always export → review → plan → modify.

### One VibeUE Service + Epic's Material Toolsets

- **LandscapeMaterialService** (`unreal.LandscapeMaterialService.*` via `execute_python`) —
  landscape-specific nodes: LayerBlend, LayerWeight, LayerCoords, LayerSample, grass output,
  LayerInfo assets, material assignment.
- **`MaterialToolset`** (via `call_tool`) — everything generic: `CreateMaterial`, `AddExpression`,
  `ConnectExpressions`, `ConnectToOutput`, `GetExpressions`, `GetExpressionInputs`, `Recompile`.
  Plus `ObjectToolset.GetProperties`/`SetProperties` for any material or expression property, and
  `AssetToolset.SaveAsset`/`Exists`.
- **MaterialNodeService** (`execute_python`) — exactly two tools left: `create_function_call` and
  `create_collection_parameter`.
- **`MaterialService` is deleted.** Every call that used to go there is now one of the two rows
  above — mind the style: VibeUE is Python, Epic is `call_tool`.

### Layer Blend Types

| Type | Use |
|------|-----|
| `LB_WeightBlend` | Standard paintable layers (most common) |
| `LB_AlphaBlend` | Non-normalized blending |
| `LB_HeightBlend` | Height-based auto-blending between layers |

### Always Create Layer Info Objects

Each layer name needs a matching `ULandscapeLayerInfoObject` asset:
- Create **before** assigning material to landscape
- Asset name convention: `LI_<LayerName>` (e.g., `LI_Grass`, `LI_Rock`)
- **ALWAYS store and use `.asset_path`** from `create_layer_info_object()` result — never guess paths
- **WRONG**: `"/Game/Landscape/Grass_LayerInfo"`  → **CORRECT**: `grass_info.asset_path` (which is `"/Game/Landscape/LI_Grass"`)

### ⚠️ assign_material_to_landscape Will Silently Skip Bad Paths

If `layer_info_paths` contains wrong paths, `assign_material_to_landscape` may return `True` (material set) but layers will be **empty**. Always verify with `get_landscape_info().layers` after assignment.

### Compile + Save After Changes

Compiling is Epic's (`MaterialService.compile_material` is gone):

```
call_tool MaterialToolset.Recompile {"MaterialOrFunction": {"refPath": "/Game/Materials/M_Terrain.M_Terrain"}}
call_tool AssetToolset.SaveAsset    {"Asset": {"refPath": "/Game/Materials/M_Terrain.M_Terrain"}}
```

(`unreal.EditorAssetLibrary.save_asset(mat_path)` from `execute_python` still saves, if you are
already mid-script.)

### ⚠️ Recompiling Is Slow — Use Separate Code Blocks

`MaterialToolset.Recompile` can take **minutes** for landscape materials with blend nodes. **NEVER** put create + compile in the same code block — it will exceed the Python execution timeout.

Split into separate steps:
1. **Block 1**: Create material, add blend nodes, add layers, connect outputs, save
2. **Block 2**: `MaterialToolset.Recompile` alone
3. **Block 3**: Create layer info objects, assign to landscape

### Texture Tiling

- Landscapes are large; textures need tiling via LandscapeLayerCoords
- Default `MappingScale=0.01` tiles every 100 units
- Smaller values = more tiling, larger values = less tiling

---


## Task Index

| Task | Sub-doc | Sample script |
|------|---------|---------------|
| Create a landscape material (+ layers) | `workflows.md` → Create Complete Landscape Material | `scripts/create_landscape_material.txt` |
| Set up a layer with textures | `workflows.md` → Setup Layer with Textures | — |
| Create layer info objects | `workflows.md` → Create Layer Info Objects | — |
| Assign material to a landscape | `workflows.md` → Assign Material to Landscape | — |
| Grass output node | `workflows.md` → Create Grass Output Node | — |
| Satellite-guided material + painting | `satellite.md` | — |

## Sub-docs
- **`workflows.md`** — step-by-step landscape-material build workflows (layers, textures, layer info, grass).
- **`satellite.md`** — satellite-image-guided material creation and slope/height painting.

## Verification
`MaterialToolset.Recompile` (slow — keep it in its own code block) then `AssetToolset.SaveAsset`.
Inspect the graph with `MaterialToolset.GetExpressions` + `GetExpressionInputs` before modifying an
existing landscape material.

## Sample scripts (run via `execute_python`)

- **`scripts/create_landscape_material.txt`** — create a landscape material with a layer blend, compile, assign.

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [satellite](satellite.md) - Satellite-guided landscape material + painting workflow — fetch a satellite image, analyze it, create satellite-informed layers, and paint by slope/he...
- [workflows](workflows.md) - Landscape material workflows — create a complete landscape material, set up layers with textures, create layer info objects, assign material to landsc...
- [scripts/create_landscape_material.txt](scripts/create_landscape_material.txt) - runnable sample
