---
name: landscape-materials
display_name: Landscape Material System
description: Create landscape materials with layer blending, texture setup, and layer info objects (LandscapeMaterialService). Use when the user asks to build a landscape/terrain material, set up paint layers (grass/rock/dirt), create layer info objects, add a grass output, or assign a material to a landscape. For production master-material+RVT systems, load landscape-auto-material.
vibeue_classes:
  - LandscapeMaterialService
  - MaterialService
  - MaterialNodeService
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

If you need material functions, Runtime Virtual Textures, or the master-material + instance pattern:
```python
vibeue-skills-manager(action="load", skill_name="landscape-auto-material")
```

## Critical Rules

### 🚨 Inspect Before Modifying Existing Landscape Materials

Before modifying an **existing** landscape material, you **MUST** export and review its current graph:

```python
import unreal, json

path = "/Game/Materials/M_Terrain"

# Step 1: Get material info
info = unreal.MaterialService.get_material_info(path)
print(f"Blend: {info.blend_mode}, Shading: {info.shading_model}")

# Step 2: Export full graph to see what nodes and connections already exist
graph = json.loads(unreal.MaterialNodeService.export_material_graph(path))
for expr in graph['expressions']:
    name = expr.get('parameter_name') or expr.get('class')
    print(f"  [{expr['id']}] {expr['class']} - {name}")
    if expr.get('landscape_layers'):
        for layer in expr['landscape_layers']:
            print(f"      Layer: {layer}")
for oc in graph['output_connections']:
    print(f"  Output '{oc['property']}' ← expr {oc['expression_id']}")
```

**Why:** Landscape materials often already have LayerBlend nodes, texture samplers, and connected outputs. Adding duplicate nodes without reviewing first creates orphaned expressions and broken connections. Always export → review → plan → modify.

### Two Services Work Together

- **LandscapeMaterialService** - Landscape-specific nodes (LayerBlend, LayerCoords, LayerInfo)
- **MaterialService + MaterialNodeService** - Standard material operations (create, compile, save, connect other nodes)

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

```python
unreal.MaterialService.compile_material(mat_path)
unreal.EditorAssetLibrary.save_asset(mat_path)
```

### ⚠️ compile_material Is Slow — Use Separate Code Blocks

`compile_material()` can take **minutes** for landscape materials with blend nodes. **NEVER** put create + compile in the same code block — it will exceed the Python execution timeout.

Split into separate steps:
1. **Block 1**: Create material, add blend nodes, add layers, connect outputs, save
2. **Block 2**: `compile_material(mat_path)` alone
3. **Block 3**: Create layer info objects, assign to landscape

### Texture Tiling

- Landscapes are large; textures need tiling via LandscapeLayerCoords
- Default `MappingScale=0.01` tiles every 100 units
- Smaller values = more tiling, larger values = less tiling

---


## Task Index

| Task | Sub-doc | Sample script |
|------|---------|---------------|
| Create a landscape material (+ layers) | `workflows.md` → Create Complete Landscape Material | `scripts/create_landscape_material.pyx` |
| Set up a layer with textures | `workflows.md` → Setup Layer with Textures | — |
| Create layer info objects | `workflows.md` → Create Layer Info Objects | — |
| Assign material to a landscape | `workflows.md` → Assign Material to Landscape | — |
| Grass output node | `workflows.md` → Create Grass Output Node | — |
| Satellite-guided material + painting | `satellite.md` | — |

## Sub-docs
- **`workflows.md`** — step-by-step landscape-material build workflows (layers, textures, layer info, grass).
- **`satellite.md`** — satellite-image-guided material creation and slope/height painting.

## Verification
`compile_material` (slow — keep it in its own code block) then save. Inspect the graph with
`MaterialNodeService.export_material_graph` before modifying an existing landscape material.

## Sample scripts (run via `execute_python_code`)

- **`scripts/create_landscape_material.pyx`** — create a landscape material with a layer blend, compile, assign.
