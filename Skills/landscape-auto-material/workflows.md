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

**Block 3 — Compile (slow!):** compiling is Epic's, via `call_tool` — `MaterialService` is gone:
```
call_tool MaterialToolset.Recompile
    {"materialOrFunction": {"refPath": "/Game/Materials/M_Terrain_Auto.M_Terrain_Auto"}}
```

### Workflow B: Create Material Instance for a New Biome

Instances and their parameters are **entirely Epic's** — `call_tool`, not Python. There is no bulk
setter any more (`MaterialService.set_instance_parameters_bulk` is gone), so **you iterate**: one
typed call per parameter, and you check each reply.

```
# 1. Create the instance under the master material
call_tool MaterialInstanceToolset.Create
    {"folderPath": "/Game/Materials", "assetName": "MI_Landscape_Tropical",
     "parent": {"refPath": "/Game/Materials/M_Landscape_Master.M_Landscape_Master"}}

# 2. Find out what the parent exposes, and each parameter's real type, before writing
call_tool MaterialInstanceToolset.ListParameters
    {"material": {"refPath": "/Game/Materials/M_Landscape_Master.M_Landscape_Master"}}

# 3. One call per parameter, typed to match (Texture / Scalar / Vector / StaticSwitch)
call_tool MaterialInstanceToolset.SetTextureParameter
    {"instance": {"refPath": "<MI>"}, "name": "GrassTexture",
     "value": {"refPath": "/Game/Textures/T_Tropical_Grass_BC.T_Tropical_Grass_BC"}}
call_tool MaterialInstanceToolset.SetTextureParameter   {"instance": {...}, "name": "RockTexture", "value": {...}}
call_tool MaterialInstanceToolset.SetTextureParameter   {"instance": {...}, "name": "SandTexture", "value": {...}}
call_tool MaterialInstanceToolset.SetScalarParameter    {"instance": {...}, "name": "SlopeThreshold",    "value": 0.7}
call_tool MaterialInstanceToolset.SetScalarParameter    {"instance": {...}, "name": "SlopeBlendWidth",   "value": 0.15}
call_tool MaterialInstanceToolset.SetScalarParameter    {"instance": {...}, "name": "AltitudeThreshold", "value": 5000.0}
call_tool MaterialInstanceToolset.SetStaticSwitchParameter {"instance": {...}, "name": "EnableSnow", "value": false}

# 4. Save, and recompile for the static switch (see "Static Switch Parameters" in SKILL.md)
call_tool AssetToolset.SaveAsset  {"assetPath": "<MI path>"}
```

A biome is now a loop you drive, not one call: nothing reports "set N of M", so the evidence for a
biome being applied is the per-parameter replies plus a `ListParameters` read-back on the instance.

### Workflow C: Create a Material Function

**Block 1 — Create function with inputs/outputs.** Function authoring is Epic's now
(`MaterialNodeService.create_material_function` / `add_function_input` / `add_function_output` are
all gone), so this block is `call_tool`. An input or output is an **expression** you add and then
configure with `ObjectToolset.SetProperties`:

```
# Create the function asset
call_tool MaterialToolset.CreateFunction {"folderPath": "/Game/Materials/Functions", "assetName": "MF_Layer_Tropical"}
call_tool ObjectToolset.SetProperties    {"instance": {"refPath": "<MF>"},
    "values": "{\"Description\": \"Samples and blends tropical layer textures\", \"bExposeToLibrary\": true}"}
# `values` is a JSON *string* (FString), not an object — json.dumps(...) it from Python.

# Inputs: one AddExpression + one SetProperties each. SortPriority sets pin order.
call_tool MaterialToolset.AddExpression {"materialOrFunction": {"refPath": "<MF>"},
    "expressionClass": "/Script/Engine.MaterialExpressionFunctionInput", "x": -600, "y": 0}
call_tool ObjectToolset.SetProperties   {"instance": {"refPath": "<that expression>"},
    "values": "{\"InputName\": \"BaseColorTexture\", \"InputType\": \"FunctionInput_Texture2D\", \"SortPriority\": 0, \"Description\": \"Diffuse texture for this layer\"}"}
# ... repeat for NormalTexture (Texture2D, 1) and Tiling (FunctionInput_Scalar, 2)

# Outputs: MaterialExpressionFunctionOutput + OutputName / SortPriority
call_tool MaterialToolset.AddExpression {"materialOrFunction": {"refPath": "<MF>"},
    "expressionClass": "/Script/Engine.MaterialExpressionFunctionOutput", "x": 400, "y": 0}
call_tool ObjectToolset.SetProperties   {"instance": {"refPath": "<that expression>"},
    "values": "{\"OutputName\": \"BlendedColor\", \"SortPriority\": 0}"}
# ... repeat for BlendedNormal (1)

# Save before adding internal nodes
call_tool AssetToolset.SaveAsset {"assetPath": "<MF path>"}
```

Keep the `refPath` each `AddExpression` returns — that is how you wire the node in Block 2. There
are no integer expression IDs any more.

**Block 2 — Build internal graph.** Also Epic's: `create_function_expression` and
`connect_function_expressions` are gone, and `MaterialToolset.AddExpression` /
`ConnectExpressions` work on a material function exactly as they do on a material.

```
# Create a texture sampler inside the function
call_tool MaterialToolset.AddExpression {"materialOrFunction": {"refPath": "<MF>"},
    "expressionClass": "/Script/Engine.MaterialExpressionTextureSample", "x": -300, "y": 0}

# Wire input -> sampler -> output, by expression refPath and PIN NAME (not index)
call_tool MaterialToolset.ConnectExpressions {"fromExpression": {"refPath": "<BaseColorTexture input>"},
    "fromOutputName": "", "toExpression": {"refPath": "<sampler>"}, "toInputName": "Tex"}
call_tool MaterialToolset.ConnectExpressions {"fromExpression": {"refPath": "<sampler>"},
    "fromOutputName": "RGB", "toExpression": {"refPath": "<BlendedColor output>"}, "toInputName": ""}

call_tool AssetToolset.SaveAsset {"assetPath": "<MF path>"}
```

`GetExpressionInputNames` / `GetExpressionOutputNames` on an expression give the exact pin names if a
connect is rejected, and `LayoutExpressions` tidies the function graph afterwards.

**Block 3 — Use function in a material:**
```python
import unreal

mat_path = "/Game/Materials/M_Landscape_Master"
func_path = "/Game/Materials/Functions/MF_Layer_Tropical"

# Create the function-call node directly (resolves the function reference for you).
# This is one of the TWO tools MaterialNodeService still has — a real VibeUE service call.
call_id = unreal.MaterialNodeService.create_function_call(
    mat_path, func_path, -600, 0)
```

Then connect it and compile with Epic's `MaterialToolset` (`call_tool`):

```
call_tool MaterialToolset.ConnectToOutput {"expression": {"refPath": "<the function-call node>"},
    "outputName": "BlendedColor", "materialProperty": "MP_BaseColor"}
call_tool MaterialToolset.Recompile       {"materialOrFunction": {"refPath": "/Game/Materials/M_Landscape_Master.M_Landscape_Master"}}
```

### Workflow D: Inspect Material Functions

`get_function_info` and `export_function_graph` are gone — there is no single info struct or JSON
dump. Walk the function with Epic's reads (`call_tool`):

```
# The function's own fields (Description, bExposeToLibrary, ...)
call_tool ObjectToolset.GetProperties   {"instance": {"refPath": "/Game/Materials/Functions/MF_AutoLayer.MF_AutoLayer"},
                                         "properties": ["Description", "bExposeToLibrary"]}
# GetProperties needs the names; ObjectToolset.ListProperties {"instance": {...}} lists them.

# Every expression in it — its inputs and outputs are the FunctionInput / FunctionOutput nodes
call_tool MaterialToolset.GetExpressions {"materialOrFunction": {"refPath": "<MF>"}}

# For each FunctionInput / FunctionOutput node, its interface fields:
call_tool ObjectToolset.GetProperties   {"instance": {"refPath": "<expression>"},
                                         "properties": ["InputName", "InputType", "SortPriority", "Description"]}
#   InputName / InputType / SortPriority / Description   (inputs)
#   OutputName / SortPriority / Description              (outputs)

# The wiring, one expression at a time
call_tool MaterialToolset.GetExpressionInputs {"materialOrFunction": {"refPath": "<MF>"},
                                               "expression": {"refPath": "<expression>"}}

# Which materials would be affected if you change this function
call_tool MaterialToolset.GetReferencingMaterials {"materialFunction": {"refPath": "<MF>"}}
```

`GetReferencingMaterials` is the check to run **before** editing a shared layer function:
`Recompile` on a function recompiles every material that uses it.

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

# Assign the RVT to the landscape — Epic's ObjectToolset on the landscape actor's
# RuntimeVirtualTextures array (write the whole array; keep any RVTs already in it).
import json
ls = next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
          if isinstance(a, unreal.Landscape) and a.get_actor_label() == "MyLandscape")
ok = await call_tool("EditorToolset.ObjectToolset.SetProperties",
                     instance={"refPath": ls.get_path_name()},
                     values=json.dumps({"RuntimeVirtualTextures": [rvt_path + ".RVT_Landscape_01"]}))
ok
```

> Epic's property write names the changed property, so `ALandscape::PostEditChangeProperty` runs its
> material rebuild and propagates the RVT list to World Partition streaming proxies — VibeUE's old
> `assign_rvt_to_landscape` fired a bare `PostEditChange()` that never reached the proxies.

**Block 3 — Add RVT output to material.** All Epic (`call_tool`): the material property, the
expression, the connections and the compile. `MaterialService.set_property` and
`compile_material` are gone.

```
# Enable virtual texturing on the material (do this BEFORE the recompile)
call_tool ObjectToolset.SetProperties {"instance": {"refPath": "/Game/Materials/M_Landscape_Master.M_Landscape_Master"},
    "values": "{\"bUsedWithVirtualTexturing\": true}"}

# Create the RVT output node
call_tool MaterialToolset.AddExpression {"materialOrFunction": {"refPath": "<master material>"},
    "expressionClass": "/Script/Engine.MaterialExpressionRuntimeVirtualTextureOutput", "x": 400, "y": 0}

# Connect BaseColor / Normal / Roughness into it — one call each, by expression refPath and pin name
# (<blend>, <normal>, <roughness> are the refPaths of your layer-blend chain's outputs)
call_tool MaterialToolset.ConnectExpressions {"fromExpression": {"refPath": "<blend>"},
    "fromOutputName": "RGB", "toExpression": {"refPath": "<rvt output>"}, "toInputName": "BaseColor"}
call_tool MaterialToolset.ConnectExpressions {"fromExpression": {"refPath": "<normal>"},
    "fromOutputName": "RGB", "toExpression": {"refPath": "<rvt output>"}, "toInputName": "Normal"}
call_tool MaterialToolset.ConnectExpressions {"fromExpression": {"refPath": "<roughness>"},
    "fromOutputName": "",    "toExpression": {"refPath": "<rvt output>"}, "toInputName": "Roughness"}

# Compile last, in its own step
call_tool MaterialToolset.Recompile {"materialOrFunction": {"refPath": "<master material>"}}
```
