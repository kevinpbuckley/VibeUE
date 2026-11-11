---
name: materials
display_name: Material System
description: Create and edit materials and material instances — graph nodes, parameters, functions, custom HLSL, and instance overrides. In UE 5.9 this is Epic's MaterialToolset / MaterialInstanceToolset / ObjectToolset via `call_tool`; VibeUE keeps exactly two graph calls (create_function_call, create_collection_parameter). Use when the user asks to create or edit a material/material instance, wire material nodes, add material parameters, set blend/shading modes, or recreate a material graph. For landscape materials load landscape-materials.
  Also use for generated organic PBR detail, texture seams, projection coordinates, and surface-normal diagnosis.
vibeue_classes:
  - MaterialNodeService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - material
  - shader
  - expression
  - node
  - parameter
  - texture
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "materials-and-shaders"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Material System Skill

For generated textures on large organic surfaces, seams after triplanar projection,
or overly strong tissue relief, read [generated organic surfaces](references/generated-organic-surfaces.md).
For gray/default surfaces after mesh reimport, use the asset-management skill's
LOD-section mapping helper before changing the shader or buying new textures.

## 🚨 Who owns what (read this first — the wrong call style just fails)

`UMaterialService` **no longer exists** (all 16 tools deleted). `UMaterialNodeService` is down to
**exactly two** calls. Everything else is Epic's, in `EditorToolset`, reached with `call_tool`.

| Call style | What it covers |
|---|---|
| **`call_tool`** → `EditorToolset.MaterialToolset` (25) | material/function assets, expressions, connections, layout, recompile, statistics, parameter groups, diffs |
| **`call_tool`** → `EditorToolset.MaterialInstanceToolset` (14) | instance create, list/get/set Scalar · Vector · Texture · StaticSwitch, `SetParent` |
| **`call_tool`** → `EditorToolset.ObjectToolset` | **any** property on a material, instance or expression — this is how you set `ParameterName`, `DefaultValue`, `Group`, `SortPriority`, `BlendMode`, a Custom node's `Code`/`OutputType`/`Inputs` |
| **`call_tool`** → `EditorToolset.AssetToolset` | `Exists`, `SaveAsset`, `Duplicate`, `Delete`, `GetReferencers`, `GetDependencies` |
| **`call_tool`** → `EditorToolset.EditorAppToolset` | `OpenEditorForAsset` |
| **`execute_python`** → `unreal.MaterialNodeService` | **only** `create_function_call` and `create_collection_parameter` |

```python
MT  = "EditorToolset.MaterialToolset"
MIT = "EditorToolset.MaterialInstanceToolset"
OT  = "EditorToolset.ObjectToolset"
AT  = "EditorToolset.AssetToolset"

def ref(p):   # "/Game/Materials/M_Foo" -> {"refPath": "/Game/Materials/M_Foo.M_Foo"}
    tail = p.rsplit("/", 1)[-1]
    return {"refPath": p if "." in tail else f"{p}.{tail}"}
```

**Three argument rules for every Epic material call:**

1. Arg names are **lowerCamelCase** of the C++ parameter (`FolderPath` → `folderPath`,
   `MaterialOrFunction` → `materialOrFunction`).
2. Every object/class argument is a **`{"refPath": "<full object path>"}`** handle, never a bare
   string — `/Game/Dir/M_X.M_X` with the asset name repeated. Expression handles come back from
   `AddExpression` / `GetExpressions` already in that shape; pass them straight through.
3. `materialProperty` is the **`EMaterialProperty` enum name** — `"MP_BaseColor"`, `"MP_Normal"`,
   `"MP_Roughness"`. The friendly `"BaseColor"` spelling that VibeUE's old service accepted is gone.

## Critical Rules

### 🚨 The two VibeUE survivors — and why they must survive

Epic's generic property tool never calls `PostEditChangeProperty`, and two node types need that hook
to become usable at all:

- **`create_function_call`** — a `MaterialFunctionCall` node's pins are populated only by
  `SetMaterialFunctionEx` → `UpdateFromFunctionResource`, reached from that hook. So
  `AddExpression` + `SetProperties({"MaterialFunction": ...})` + `Recompile` yields a **pin-less node
  that cannot be wired**. There is no other way.
- **`create_collection_parameter`** — same shape: the node's `ParameterId` GUID is derived in that
  hook by looking the name up in the collection. A missing id compiles to
  `"CollectionParameter has invalid parameter"`.

```python
# execute_python — these two are VibeUE, NOT call_tool.
fn = unreal.MaterialNodeService.create_function_call(
    "/Game/Materials/M_Rock",
    "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture", -600, 0)
coll = unreal.MaterialNodeService.create_collection_parameter(
    "/Game/Materials/M_Rock", "/Game/Materials/MPC_GlobalParams", "WindStrength", -600, 300)
```

Both return a `MaterialExpressionInfo`: read `.id` (session-specific) or `.object_path` (durable),
plus `.input_names` / `.output_names`. To hand one of these nodes to an Epic call, wrap its
`object_path`: `{"refPath": fn.object_path}`. Passing the struct itself where a string is expected
gives `TypeError: Nativize: Cannot nativize 'MaterialExpressionInfo' as 'String'`.

Every other node — parameters, math, texture samples, Custom HLSL — is `AddExpression`.

### ✅ Pin names: what you read is what connect accepts

`MaterialToolset.GetExpressionInputNames` and `MaterialToolset.ConnectExpressions` are both driven
from `UMaterialEditingLibrary`'s **bare-name** matching, so the names you read back are exactly the
names the connect call accepts — no display labels, no type suffixes, no translation step. And a
connect that does not land now **raises** (`Failed to connect output "X" on A to input "Y" on B`)
instead of silently producing a phantom wire. Read, connect, trust it.

```python
names = call_tool(tool_name="GetExpressionInputNames", toolset_name=MT,
                  arguments={"expression": fn_ref})["returnValue"]
# -> ["TextureObject", "TextureSize", "WorldPosition", ...]
call_tool(tool_name="ConnectExpressions", toolset_name=MT,
          arguments={"fromExpression": tex_ref, "fromOutputName": "",
                     "toExpression": fn_ref, "toInputName": names[0]})
```

`GetExpressionOutputNames` is the same for the other end; `""` means the default (first) output.
`GetExpressionInputs(materialOrFunction, expression)` reads back the live wiring, one entry per
input pin, with `expression: null` for an unwired pin.

### 🚨 Diagnostics: `Recompile` raises, `GetStatistics` counts

There is no `get_material_diagnostics` and no `get_used_textures` workaround any more — 5.9 reports
both halves directly:

- **`MaterialToolset.Recompile`** raises on a compile failure with every error **localized to the
  failing node**: `Material failed to compile:\nnode '/Game/M_Rock.M_Rock:MaterialExpressionTextureSample_0': Sampler type is Color, should be Masks for ...`.
  A clean compile returns quietly. Call it once after a batch of graph edits, not per node.
- **`MaterialToolset.GetStatistics`** returns the real **post-compile** counts from the shader map:
  `num_pixel_texture_samples`, `num_samplers`, `num_pixel_shader_instructions`,
  `num_vertex_shader_instructions`, `num_vertex_texture_samples`, `num_virtual_texture_samples`,
  `num_uv_scalars`, `num_interpolator_scalars`.

```python
call_tool(tool_name="Recompile", toolset_name=MT, arguments={"materialOrFunction": ref(path)})
stats = call_tool(tool_name="GetStatistics", toolset_name=MT,
                  arguments={"material": ref(path)})["returnValue"]
assert stats["num_pixel_texture_samples"] > 0   # a texturing material that samples nothing is broken
call_tool(tool_name="SaveAsset", toolset_name=AT, arguments={"assetPath": path})
```

`num_pixel_texture_samples == 0` on a material that is supposed to sample textures is the honest
signal that the texture branch never reaches an output. `GetStatistics` also works on a
MaterialInstance (it takes a `UMaterialInterface`), so an instance's static-switch permutation can be
measured the same way.

Visual confirmation: `call_tool("OpenEditorForAsset", "EditorToolset.EditorAppToolset", {"assetPath": path})`,
then capture with `EditorAppToolset` and `attach_image(...)` the result.

> ⚠️ Best-effort focus: with many asset editors open as tabs, the editor tab may not switch
> reliably. Close the others first if the screenshot keeps catching the wrong tab:
>
> ```python
> ed = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
> for p in other_open_paths:
>     ed.close_all_editors_for_asset(unreal.load_asset(p))
> ```

### 🚨 Every property — material, instance or expression — goes through `ObjectToolset`

`ObjectToolset.SetProperties` takes the **instance** plus a **JSON string** of values. This is the
only property writer left: there is no `MaterialService.set_property`, no `set_expression_property`,
no `set_parameter_metadata`.

```python
import json
# A parameter expression's identity and default.
call_tool(tool_name="SetProperties", toolset_name=OT, arguments={
    "instance": vec_ref,
    "values": json.dumps({"ParameterName": "BaseColor", "Group": "Surface",
                          "SortPriority": 0,
                          "DefaultValue": {"r": 0.8, "g": 0.2, "b": 0.2, "a": 1.0}})})

# The material's own render state. Enum values are the full prefixed identifiers.
call_tool(tool_name="SetProperties", toolset_name=OT, arguments={
    "instance": ref(path),
    "values": json.dumps({"BlendMode": "BLEND_Masked", "OpacityMaskClipValue": 0.33,
                          "TwoSided": True})})
call_tool(tool_name="Recompile", toolset_name=MT, arguments={"materialOrFunction": ref(path)})
```

- Internal property names only (`TwoSided`, `BlendMode`, `ShadingModel`, `MaterialDomain`,
  `OpacityMaskClipValue`, `bUsedWithStaticLighting`, `DitheredLODTransition`). The old display-name
  tolerance (`"Two Sided"`, `"Blend Mode"`) went with `MaterialService`.
- Enum values are the prefixed identifiers: `BLEND_Opaque` / `BLEND_Masked` / `BLEND_Translucent`,
  `MSM_DefaultLit`, `MD_Surface`. **Discover, don't guess:** `ObjectToolset.ListProperties` finds the
  property name, then `GetPropertySchema` gives its type and legal values. `ListProperties` defaults
  to `maxDepth: 1` — pass `0` to search every level, and a `nameFilter` regex to stay small.
- `SetProperties` returns a **bool**. Check it, or read back with `GetProperties`
  (`{"instance": ..., "properties": ["BlendMode"]}`).
- Changing a parameter's `ParameterName` after instances exist orphans their overrides — the override
  is keyed by name. Rename on the parent and the children silently fall back to the default.

### 🚨 Inspect before modifying an existing material

There is no `export_material_graph`. Read the graph with the Epic trio before you touch it:

```python
exprs = call_tool(tool_name="GetExpressions", toolset_name=MT,
                  arguments={"materialOrFunction": ref(path)})["returnValue"]
for e in exprs:
    props = call_tool(tool_name="GetProperties", toolset_name=OT,
                      arguments={"instance": e, "properties": ["ParameterName", "Group"]})
    ins = call_tool(tool_name="GetExpressionInputs", toolset_name=MT,
                    arguments={"materialOrFunction": ref(path), "expression": e})["returnValue"]
    print(e["refPath"], props, [(i["inputName"], i["expression"]) for i in ins])

# What currently drives an output, before you overwrite it:
src = call_tool(tool_name="GetPropertyInput", toolset_name=MT,
                arguments={"material": ref(path), "materialProperty": "MP_BaseColor"})["returnValue"]
```

Then: plan → `DisconnectExpressions` / `DisconnectFromOutput` what you are replacing → `AddExpression`
only the genuinely new nodes → `ConnectExpressions` / `ConnectToOutput` → `LayoutExpressions` →
`Recompile` → `AssetToolset.SaveAsset`. `DeleteUnusedExpressions` sweeps up abandoned nodes (it does
**not** recompile — follow it with `Recompile`). `MaterialToolset.DiffMaterial(oldAssetPath,
newAssetPath)` gives a readable text diff of two material assets, and `DiffFunction` the same for
functions — the fastest way to confirm an edit changed only what you intended.

### ✅ `LayoutExpressions` exists — auto-arrange the graph

Earlier versions of this pack said there is no layout API. **That was wrong for 5.9.**
`MaterialToolset.LayoutExpressions` auto-arranges every node in a material or function graph
(`UMaterialEditingLibrary::LayoutMaterialExpressions`). You still pass `x`/`y` to `AddExpression`, but
you no longer have to get them right — tidy up afterwards:

```python
call_tool(tool_name="LayoutExpressions", toolset_name=MT,
          arguments={"materialOrFunction": ref(path)})
```

### ⚠️ Creating nodes: `AddExpression` takes a class **path**, not a bare name

`expressionClass` is a `UClass` handle, so it is a `{"refPath": "/Script/Engine.MaterialExpression<X>"}`.
The old bare-name-minus-prefix form (`"Multiply"`) is not accepted. Discover the exact paths with
`ListExpressionClasses` (it filters by context: `FunctionInput`/`FunctionOutput` appear only for a
MaterialFunction, never for a Material):

```python
classes = call_tool(tool_name="ListExpressionClasses", toolset_name=MT,
                    arguments={"materialOrFunction": ref(path), "search": "Multiply"})["returnValue"]
mult = call_tool(tool_name="AddExpression", toolset_name=MT,
                 arguments={"materialOrFunction": ref(path),
                            "expressionClass": {"refPath": classes[0]},
                            "x": -300, "y": 0})["returnValue"]
```

### ⚠️ Parameter expressions and their properties

A parameter is `AddExpression` of the parameter class, then `ObjectToolset.SetProperties`.

| Parameter | `expressionClass` refPath (`/Script/Engine.` +) | Properties to set |
|---|---|---|
| Scalar | `MaterialExpressionScalarParameter` | `ParameterName`, `Group`, `DefaultValue` (float) |
| Vector / colour | `MaterialExpressionVectorParameter` | `ParameterName`, `Group`, `DefaultValue` (`{r,g,b,a}`) |
| Texture (with sampler) | `MaterialExpressionTextureSampleParameter2D` | `ParameterName`, `Texture` (refPath), **`SamplerType`** |
| Texture object (no sampler) | `MaterialExpressionTextureObjectParameter` | `ParameterName`, `Texture` (refPath), **`SamplerType`** |
| Static switch | `MaterialExpressionStaticSwitchParameter` | `ParameterName`, `DefaultValue` (bool) |
| Collection parameter | — | **VibeUE `create_collection_parameter`** (see above) |
| Material function call | — | **VibeUE `create_function_call`** (see above) |

> 🚨 **`SamplerType` is the single most common compile failure.** The default is
> `SAMPLERTYPE_Color`, which is wrong for any data texture. ORM / mask / packed maps need
> `SAMPLERTYPE_Masks` or `SAMPLERTYPE_LinearColor`; a tangent normal map needs
> `SAMPLERTYPE_Normal`; a single-channel linear mask needs `SAMPLERTYPE_LinearGrayscale`. Get it
> wrong and `Recompile` raises `"Sampler type is Color, should be Masks for /Game/.../T_X_ORM"`.
> Set it in the same `SetProperties` call as `Texture`.

Pin names worth knowing (they are not guessable): `StaticSwitchParameter` inputs are `True` / `False`;
`LandscapeLayerBlend` inputs are `"Layer <name>"` / `"Height <name>"`, and `LayerBlendInput` fields are
invisible to `dir()` but `set_editor_property("layer_name" | "blend_type" | "preview_weight")` works.

### ⚠️ Custom HLSL: `AddExpression` + `SetProperties`

There is no `create_custom_expression`. A Custom node is a plain expression whose `Code`,
`OutputType` and `Inputs` are properties — and because it has no pin-generating `PostEditChange`
dependency, the generic path works:

```python
custom = call_tool(tool_name="AddExpression", toolset_name=MT, arguments={
    "materialOrFunction": ref(path),
    "expressionClass": {"refPath": "/Script/Engine.MaterialExpressionCustom"},
    "x": -500, "y": 0})["returnValue"]
call_tool(tool_name="SetProperties", toolset_name=OT, arguments={
    "instance": custom,
    "values": json.dumps({
        "Code": "return sin(Time * Speed);",
        "Description": "SineWave",
        "OutputType": "CMOT_Float1",                       # CMOT_Float1..4, CMOT_MaterialAttributes
        "Inputs": [{"InputName": "Time"}, {"InputName": "Speed"}]})})
```

`Inputs` is a container, so **set the whole array** to add or remove pins — a subscript only
addresses an element that already exists. The input pin names you just declared are what
`ConnectExpressions` takes as `toInputName`. Extra outputs go in `AdditionalOutputs` as
`[{"OutputName": "Mask", "OutputType": "CMOT_Float3"}]`.

### ⚠️ Material output properties (`EMaterialProperty`)

`MP_BaseColor`, `MP_Metallic`, `MP_Specular`, `MP_Roughness`, `MP_EmissiveColor`, `MP_Normal`,
`MP_Opacity`, `MP_OpacityMask`, `MP_WorldPositionOffset`, `MP_AmbientOcclusion`.

```python
call_tool(tool_name="ConnectToOutput", toolset_name=MT,
          arguments={"expression": mult, "outputName": "", "materialProperty": "MP_BaseColor"})
call_tool(tool_name="DisconnectFromOutput", toolset_name=MT,
          arguments={"material": ref(path), "materialProperty": "MP_BaseColor"})
```

### 🚨 Tiling on basic shapes: prefer a child MI with a `Tiling` override

For Megascans `M_MS_Srf` and similar surface materials with a `Tiling` scalar, the cleanest
per-actor tiling fix is a **child material instance with `Tiling` overridden** — *not* a UV transform
on the mesh. UV transforms apply to all faces uniformly and break orientation; the parent material's
UV mapping is already correct per face.

```python
# ✅ "this disc shows brick too sparse / too dense"
# NOTE MaterialInstanceToolset.Create arg order: folderPath, assetName, parent.
mi = call_tool(tool_name="Create", toolset_name=MIT, arguments={
    "folderPath": "/Game/Materials/", "assetName": "MI_xevtfjz_Disc",
    "parent": ref("/Game/Fab/Megascans/.../MI_xevtfjz")})["returnValue"]
call_tool(tool_name="SetScalarParameter", toolset_name=MIT,
          arguments={"instance": mi, "name": "Tiling", "value": 4.0})
call_tool(tool_name="SaveAsset", toolset_name=AT,
          arguments={"assetPath": "/Game/Materials/MI_xevtfjz_Disc"})
# then assign it to the actor
```

For triplanar / world-aligned (the same material on cube, sphere and cylinder with no UV concerns),
build a master material wrapping `WorldAlignedTexture` / `WorldAlignedNormal` (functions under
`/Engine/Functions/Engine_MaterialFunctions01/Texturing/`) — via **`create_function_call`**, since a
function call node is one of the two survivors. Feed its `TextureObject` input from a
`MaterialExpressionTextureObjectParameter` (**not** a `TextureSampleParameter2D` — the function brings
its own sampler). Feed `TextureSize` from a `Constant3Vector`, or `Multiply(Scalar, Constant3Vector(1,1,1))`;
**a bare scalar broadcasts incorrectly to V3** in that input slot.

---

## Task Index

| Task | Workflow | Sample script |
|------|----------|---------------|
| Create a material (+ params) | `workflows.md` → Create a basic material | `scripts/create_material.txt` |
| Create a material instance | `workflows.md` → Create a material instance | `scripts/create_instance.txt` |
| Add texture / math / function / HLSL nodes | `workflows.md` | — |
| Build a multi-node graph | `workflows.md` → Build a graph | `scripts/material_graph_batch.txt` |
| Inspect / dump an existing graph | `reference.md` → Reading a graph | `scripts/export_graph.txt` |
| Clone a material and edit the copy | `reference.md` → Duplicate and retarget | — |
| Verify wiring / compile | Critical Rules → `Recompile` + `GetStatistics` | — |

## Sub-docs

- **`workflows.md`** — create material/instance, texture/math/function/HLSL/collection/switch nodes,
  set properties, and build a graph node by node.
- **`reference.md`** — the full tool inventory, parameter expression classes, output names, enum
  formats, reading a graph, duplicate-and-retarget, and Material Function authoring.

## Verification

After non-trivial wiring: `MaterialToolset.Recompile` (it raises with the failing node's path), then
`MaterialToolset.GetStatistics` and confirm `num_pixel_texture_samples` / `num_samplers` match what
the graph should sample. `AssetToolset.SaveAsset` to persist.

## Related skills
- **landscape-materials** — `LandscapeLayerBlend` nodes.
- **landscape-auto-material** — production landscape materials (functions, RVT, instances).

## Additional gotchas

- `StaticMesh.get_editor_property("static_materials")` returns struct COPIES, so writing the array back is a silent no-op; use `set_material(index, mat)` and confirm with `get_material(index)`.
- `MEL.set_material_instance_scalar_parameter_value` returns False on success, and the static-switch setter returns False while the write still lands — prefer `MaterialInstanceToolset.SetScalarParameter` / `SetStaticSwitchParameter`, which raise instead of lying. Either way, MI reads return 0.0 and writes no-op while PIE runs.
- Changing `body_setup.collision_trace_flag` does not rebuild the physics mesh; toggle `StaticMeshEditorSubsystem.enable_section_collision` off then on.
- `MaterialToolset.GetExpressions` is the reliable enumeration. In raw Python, `ObjectIterator(unreal.MaterialExpression)` filtered on `path.startswith(mat_path + ":")` also works (class-specific iterators miss nodes). `MaterialToolset.DeleteExpression` enables in-place rebuilds that keep instance overrides when parameter names match.
- Gate optional master-material features behind STATIC switches (a scalar "off" still samples); measure with `MaterialToolset.GetStatistics`. A material used on a Niagara sprite renderer needs `bUsedWithNiagaraSprites`.
- `AssetToolset.Delete` is the normal delete. For a material that may still be referenced, `unreal.AssetDiscoveryService.delete_asset_unattended` refuses and returns a result struct naming the holder (including a pinning Python global) instead of wedging on a modal dialog. `AssetToolset.GetReferencers` tells you who holds it first.

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [reference](reference.md) - Material reference — the UE 5.9 tool inventory, parameter expression classes, material output names, enum formats, reading a graph, duplicate-and-ret...
- [workflows](workflows.md) - Step-by-step material authoring workflows — create a material, material instance, texture/math/function-call/custom-HLSL/collection/static-switch node...
- [scripts/create_instance.txt](scripts/create_instance.txt) - runnable sample
- [scripts/create_material.txt](scripts/create_material.txt) - runnable sample
- [scripts/export_graph.txt](scripts/export_graph.txt) - runnable sample
- [scripts/material_graph_batch.txt](scripts/material_graph_batch.txt) - runnable sample
