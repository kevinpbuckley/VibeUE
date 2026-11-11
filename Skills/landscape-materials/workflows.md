---
name: landscape-materials/workflows
description: Landscape material workflows — create a complete landscape material, set up layers with textures, create layer info objects, assign material to landscape, layer-weight/coords/sample nodes, grass output, and a full end-to-end build.
---

## Workflows

### Create Complete Landscape Material

**IMPORTANT: Split into multiple code blocks to avoid timeout!**

**Block 1 — Create and connect (fast):**
```python
import unreal

# 1. Create material
mat = unreal.LandscapeMaterialService.create_landscape_material("M_Terrain", "/Game/Materials")
mat_path = mat.asset_path

# 2. Create layer blend node
blend = unreal.LandscapeMaterialService.create_layer_blend_node(mat_path, -400, 0)

# 3. Add layers
unreal.LandscapeMaterialService.add_layer_to_blend_node(mat_path, blend.node_id, "Grass", "LB_WeightBlend")
unreal.LandscapeMaterialService.add_layer_to_blend_node(mat_path, blend.node_id, "Rock", "LB_WeightBlend")
unreal.LandscapeMaterialService.add_layer_to_blend_node(mat_path, blend.node_id, "Sand", "LB_WeightBlend")

# 4. Save (but do NOT compile here — compile is slow!)
unreal.EditorAssetLibrary.save_asset(mat_path)
print(f"Created: {mat_path}")
```

**Step 5 — connect the blend to the BaseColor output.** That is Epic's, so it is `call_tool`, and it
needs the **expression object**, not `blend.node_id`:

```
# Find the LandscapeLayerBlend expression (match on its class), then wire it to BaseColor
call_tool MaterialToolset.GetExpressions  {"MaterialOrFunction": {"refPath": "/Game/Materials/M_Terrain.M_Terrain"}}
call_tool MaterialToolset.ConnectToOutput {"Expression": {"refPath": "<the LandscapeLayerBlend expression>"},
                                           "OutputName": "", "MaterialProperty": "MP_BaseColor"}
```

> ⚠️ **VibeUE node ids and Epic expression refs are different things and do not interoperate.**
> A `node_id` from `LandscapeMaterialService` is an internal `<Class>_<pointer>` handle: pass it only
> back to `LandscapeMaterialService`. Epic's `MaterialToolset` takes an object reference
> (`{"refPath": ...}`). To cross over, list the graph with `MaterialToolset.GetExpressions` and pick
> the node by its class — there is no id-to-ref converter (`MaterialNodeService.list_expressions` is
> gone).

**Block 2 — Compile (slow, run separately).** Epic's, via `call_tool` —
`MaterialService.compile_material` is gone:
```
call_tool MaterialToolset.Recompile {"MaterialOrFunction": {"refPath": "/Game/Materials/M_Terrain.M_Terrain"}}
```

**Block 3 — Layer info objects and assignment:**
```python
import unreal

# Create layer info objects — ALWAYS use .asset_path, naming is LI_<Name>
grass_info = unreal.LandscapeMaterialService.create_layer_info_object("Grass", "/Game/Landscape")
rock_info = unreal.LandscapeMaterialService.create_layer_info_object("Rock", "/Game/Landscape")
sand_info = unreal.LandscapeMaterialService.create_layer_info_object("Sand", "/Game/Landscape")
# grass_info.asset_path = "/Game/Landscape/LI_Grass" (NOT "Grass_LayerInfo")

# Assign material with EXACT paths from create results
unreal.LandscapeMaterialService.assign_material_to_landscape(
    "MyTerrain", "/Game/Materials/M_Terrain",
    {
        "Grass": grass_info.asset_path,
        "Rock": rock_info.asset_path,
        "Sand": sand_info.asset_path
    }
)

# VERIFY layers were added (assign can silently skip bad paths)
info = unreal.LandscapeService.get_landscape_info("MyTerrain")
print(f"Layers: {[l.layer_name for l in info.layers]}")
```

### Setup Layer with Textures

```python
import unreal

mat_path = "/Game/Materials/M_Terrain"
blend_node_id = "..."  # from create_layer_blend_node

# Setup all textures for a layer in one call
unreal.LandscapeMaterialService.setup_layer_textures(
    mat_path,
    blend_node_id,
    "Grass",
    "/Game/Textures/T_Grass_D",      # Diffuse
    "/Game/Textures/T_Grass_N",      # Normal (optional)
    "",                               # Roughness (optional)
    0.01                              # Tiling scale
)

# Compile: Epic's MaterialToolset owns this (MaterialService is gone) — run it as its own step:
#   call_tool MaterialToolset.Recompile {"MaterialOrFunction": {"refPath": "<mat_path>.<MatName>"}}
# (mid-script equivalent: vibeue.exec_tool("EditorToolset.MaterialToolset", "Recompile", {...}))
```

### Create Layer Info Objects

```python
import unreal

# Create layer info objects for each layer
grass_info = unreal.LandscapeMaterialService.create_layer_info_object("Grass", "/Game/Landscape", True)
rock_info = unreal.LandscapeMaterialService.create_layer_info_object("Rock", "/Game/Landscape", True)
sand_info = unreal.LandscapeMaterialService.create_layer_info_object("Sand", "/Game/Landscape", True)

print(f"Grass: {grass_info.asset_path}")
print(f"Rock: {rock_info.asset_path}")
print(f"Sand: {sand_info.asset_path}")
```

### Assign Material to Landscape

```python
import unreal

# IMPORTANT: Use EXACT paths from create_layer_info_object().asset_path
# The naming convention is LI_<LayerName>, NOT <LayerName>_LayerInfo
# If paths are wrong, assign returns True but layers will be EMPTY!
unreal.LandscapeMaterialService.assign_material_to_landscape(
    "MyTerrain",
    "/Game/Materials/M_Terrain",
    {
        "Grass": "/Game/Landscape/LI_Grass",
        "Rock": "/Game/Landscape/LI_Rock.LI_Rock",
        "Sand": "/Game/Landscape/LI_Sand.LI_Sand"
    }
)

# Always verify after assignment
info = unreal.LandscapeService.get_landscape_info("MyTerrain")
if not info.layers:
    print("WARNING: No layers found - check layer_info_paths!")
```

### Manual Texture Connection with Layer Coords

**Prefer `LandscapeMaterialService.setup_layer_textures`** (above): it does the sampler, the coords
and the layer wiring in one VibeUE call, with no id/ref crossing. Do it by hand only when you need a
node `setup_layer_textures` does not build.

By hand, keep each node on **one** side of the fence (see the warning above):
`MaterialNodeService.list_expressions` and `set_expression_property` are gone, so the sampler, its
texture and its connections are all Epic's.

```
# UV coords: VibeUE, from execute_python (returns a VibeUE node id)
#   coords_id = unreal.LandscapeMaterialService.create_layer_coords_node(mat_path, 0.01, -800, 0)

# Texture sample: Epic. AddExpression returns the expression object — keep its refPath.
call_tool MaterialToolset.AddExpression {"MaterialOrFunction": {"refPath": "/Game/Materials/M_Terrain.M_Terrain"},
    "ExpressionClass": "/Script/Engine.MaterialExpressionTextureSample", "X": -600, "Y": 0}

# Its texture is just a property (set_expression_property is gone):
call_tool ObjectToolset.SetProperties {"Object": {"refPath": "<the TextureSample>"},
    "Properties": {"Texture": {"refPath": "/Game/Textures/T_Grass_D.T_Grass_D"}}}

# Wire sampler -> the LayerBlend's layer input, Epic side, by PIN NAME.
# GetExpressions finds the LandscapeLayerBlend; GetExpressionInputNames on it gives the exact
# per-layer pin spelling (e.g. "Layer Grass").
call_tool MaterialToolset.GetExpressionInputNames {"Expression": {"refPath": "<the LandscapeLayerBlend>"}}
call_tool MaterialToolset.ConnectExpressions {"FromExpression": {"refPath": "<the TextureSample>"},
    "FromOutputName": "RGB", "ToExpression": {"refPath": "<the LandscapeLayerBlend>"}, "ToInputName": "<layer pin>"}
```

`LandscapeMaterialService.connect_to_layer_input(mat_path, node_id, "", blend_node_id, "Grass",
"Layer")` is still the right call when **both** nodes came from `LandscapeMaterialService` and you
still hold their ids — e.g. a layer coords node feeding a layer sample node.

### Using Layer Weight Nodes (Alternative)

```python
import unreal

mat_path = "/Game/Materials/M_Terrain"

# LandscapeLayerWeight is an alternative to LandscapeLayerBlend
# Use when you need more control over blending logic
grass_weight = unreal.LandscapeMaterialService.create_layer_weight_node(mat_path, "Grass", 1.0, -400, 0)
rock_weight = unreal.LandscapeMaterialService.create_layer_weight_node(mat_path, "Rock", 0.0, -400, 200)

# Connect layer weight inputs and chain them together
# (more manual but more flexible than LayerBlend)
```

### Check Existence

```python
import unreal

if not unreal.LandscapeMaterialService.landscape_material_exists("/Game/Materials/M_Terrain"):
    unreal.LandscapeMaterialService.create_landscape_material("M_Terrain", "/Game/Materials")

if not unreal.LandscapeMaterialService.layer_info_exists("/Game/Landscape/LI_Grass"):
    unreal.LandscapeMaterialService.create_layer_info_object("Grass", "/Game/Landscape")
```

### Create Layer Sample Node

Use `LandscapeLayerSample` to sample a single layer's weight (alternative to LayerBlend/LayerWeight):

```python
import unreal

mat_path = "/Game/Materials/M_Terrain"

# Creates a LandscapeLayerSample node for a specific layer
# Returns the scalar weight of that layer (0-1)
grass_sample = unreal.LandscapeMaterialService.create_layer_sample_node(
    mat_path, "Grass", -600, 0
)

print("layer sample node id:", grass_sample)
```

To drive blending or masking with that weight, add the Multiply on Epic's side and wire it there.
`grass_sample` is a VibeUE node id, so find the LayerSample expression by class to get its ref — the
id itself is not accepted by `MaterialToolset`:

```
call_tool MaterialToolset.AddExpression {"MaterialOrFunction": {"refPath": "/Game/Materials/M_Terrain.M_Terrain"},
    "ExpressionClass": "/Script/Engine.MaterialExpressionMultiply", "X": -400, "Y": 0}
call_tool MaterialToolset.GetExpressions {"MaterialOrFunction": {"refPath": "<material>"}}   # find the LandscapeLayerSample
call_tool MaterialToolset.ConnectExpressions {"FromExpression": {"refPath": "<the LandscapeLayerSample>"},
    "FromOutputName": "", "ToExpression": {"refPath": "<the Multiply>"}, "ToInputName": "A"}
```

### Create Grass Output Node

Create a `LandscapeGrassOutput` node for procedural foliage placement:

```python
import unreal

mat_path = "/Game/Materials/M_Terrain"

# Create grass output with grass type mappings
# Keys are display names, values are ULandscapeGrassType asset paths
grass_id = unreal.LandscapeMaterialService.create_grass_output(
    mat_path,
    {
        "Grass": "/Game/Foliage/GT_Grass",
        "Flowers": "/Game/Foliage/GT_Flowers"
    },
    -200, 400
)

print("grass output node id:", grass_id)
```

Connecting the layer weights into it is Epic's (`call_tool`). Each grass type has an input pin named
after it; both the grass output and the layer-weight nodes came from `LandscapeMaterialService`, so
look them up by class with `GetExpressions` to get their refs:

```
call_tool MaterialToolset.GetExpressions {"MaterialOrFunction": {"refPath": "/Game/Materials/M_Terrain.M_Terrain"}}
call_tool MaterialToolset.ConnectExpressions {"FromExpression": {"refPath": "<Grass layer-weight node>"},
    "FromOutputName": "", "ToExpression": {"refPath": "<the LandscapeGrassOutput>"}, "ToInputName": "Grass"}
call_tool MaterialToolset.ConnectExpressions {"FromExpression": {"refPath": "<Flowers layer-weight node>"},
    "FromOutputName": "", "ToExpression": {"refPath": "<the LandscapeGrassOutput>"}, "ToInputName": "Flowers"}

# Then compile, as its own step:
call_tool MaterialToolset.Recompile {"MaterialOrFunction": {"refPath": "<material>"}}
```

### Complete End-to-End Workflow

```python
import unreal

# 1. Create landscape material
mat = unreal.LandscapeMaterialService.create_landscape_material("M_Terrain", "/Game/Materials")
mat_path = mat.asset_path

# 2. Create blend node and add layers
blend = unreal.LandscapeMaterialService.create_layer_blend_node(mat_path)
unreal.LandscapeMaterialService.add_layer_to_blend_node(mat_path, blend.node_id, "Grass")
unreal.LandscapeMaterialService.add_layer_to_blend_node(mat_path, blend.node_id, "Rock")

# 3. Setup textures for each layer
unreal.LandscapeMaterialService.setup_layer_textures(
    mat_path, blend.node_id, "Grass", "/Game/Textures/T_Grass_D", "/Game/Textures/T_Grass_N")
unreal.LandscapeMaterialService.setup_layer_textures(
    mat_path, blend.node_id, "Rock", "/Game/Textures/T_Rock_D", "/Game/Textures/T_Rock_N")

# 4. Save the graph so far
unreal.EditorAssetLibrary.save_asset(mat_path)

# --- STEPS 5 and 6 ARE EPIC'S: two call_tool calls, each its own step, NOT Python ---
#   a) connect the blend to BaseColor (needs the EXPRESSION, not blend.node_id — find the
#      LandscapeLayerBlend in MaterialToolset.GetExpressions):
#        call_tool MaterialToolset.ConnectToOutput
#            {"Expression": {"refPath": "<LandscapeLayerBlend>"}, "OutputName": "",
#             "MaterialProperty": "MP_BaseColor"}
#   b) compile (slow — on its own):
#        call_tool MaterialToolset.Recompile {"MaterialOrFunction": {"refPath": "<mat_path>.<MatName>"}}
# Then continue with the Python below.

# 6. Create layer info objects — ALWAYS use .asset_path result
grass_info = unreal.LandscapeMaterialService.create_layer_info_object("Grass", "/Game/Landscape")
rock_info = unreal.LandscapeMaterialService.create_layer_info_object("Rock", "/Game/Landscape")
# grass_info.asset_path = "/Game/Landscape/LI_Grass"

# 7. Create landscape
ls = unreal.LandscapeService.create_landscape(
    unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), unreal.Vector(100, 100, 100),
    1, 63, 8, 8, "MyTerrain")

# 8. Assign material with layer info
unreal.LandscapeMaterialService.assign_material_to_landscape(
    ls.actor_label, mat_path,
    {"Grass": grass_info.asset_path, "Rock": rock_info.asset_path})

print("Landscape setup complete!")
```

---

