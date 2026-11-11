---
name: materials/workflows
description: Step-by-step material authoring workflows for UE 5.9 — create a material, material instance, texture/math/function-call/custom-HLSL/collection/static-switch nodes, set material and expression properties, and build a graph node by node with Epic's MaterialToolset.
---

# Material Workflows

## Contents
- Create a basic material
- Create a material instance
- Add a texture parameter
- Create a math expression
- Set material properties
- Function call node (MaterialFunction)
- Custom HLSL expression
- Collection parameter
- Static switch / texture object parameter
- Build a graph
- Tidy and verify

In UE 5.9 material authoring is **Epic's**, via `call_tool`:
`EditorToolset.MaterialToolset` (materials and functions), `EditorToolset.MaterialInstanceToolset`
(instances), `EditorToolset.ObjectToolset` (every property), `EditorToolset.AssetToolset` (exists /
save / duplicate / delete / referencers). `UMaterialService` is gone; `unreal.MaterialNodeService`
keeps exactly two calls — `create_function_call` and `create_collection_parameter` — run through
`execute_python`, not `call_tool`.

Every object/class argument is a `{"refPath": "/Game/Dir/Asset.Asset"}` handle; arg names are
lowerCamelCase; `materialProperty` is the `EMaterialProperty` name (`"MP_BaseColor"`). All examples
below assume this preamble. Runnable versions in `scripts/`.

```python
import json
MT  = "EditorToolset.MaterialToolset"
MIT = "EditorToolset.MaterialInstanceToolset"
OT  = "EditorToolset.ObjectToolset"
AT  = "EditorToolset.AssetToolset"
E   = "/Script/Engine."                      # expression class prefix

def ref(p):
    tail = p.rsplit("/", 1)[-1]
    return {"refPath": p if "." in tail else f"{p}.{tail}"}

def add(path, cls, x, y):
    return call_tool(tool_name="AddExpression", toolset_name=MT,
                     arguments={"materialOrFunction": ref(path), "expressionClass": {"refPath": E + cls},
                                "x": x, "y": y})["returnValue"]

def set_props(obj, values):
    return call_tool(tool_name="SetProperties", toolset_name=OT,
                     arguments={"instance": obj, "values": json.dumps(values)})
```

## Create a basic material

```python
path = "/Game/Materials/M_Character"
call_tool(tool_name="CreateMaterial", toolset_name=MT,
          arguments={"folderPath": "/Game/Materials/", "assetName": "M_Character"})

color = add(path, "MaterialExpressionVectorParameter", -500, 0)
set_props(color, {"ParameterName": "BaseColor", "Group": "Surface",
                  "DefaultValue": {"r": 0.8, "g": 0.2, "b": 0.2, "a": 1.0}})
call_tool(tool_name="ConnectToOutput", toolset_name=MT,
          arguments={"expression": color, "outputName": "", "materialProperty": "MP_BaseColor"})

rough = add(path, "MaterialExpressionScalarParameter", -500, 150)
set_props(rough, {"ParameterName": "Roughness", "Group": "Surface", "DefaultValue": 0.5})
call_tool(tool_name="ConnectToOutput", toolset_name=MT,
          arguments={"expression": rough, "outputName": "", "materialProperty": "MP_Roughness"})

call_tool(tool_name="LayoutExpressions", toolset_name=MT, arguments={"materialOrFunction": ref(path)})
call_tool(tool_name="Recompile", toolset_name=MT, arguments={"materialOrFunction": ref(path)})
call_tool(tool_name="SaveAsset", toolset_name=AT, arguments={"assetPath": path})
```

`CreateMaterial` returns the new material as a `{refPath}` handle, so you can use its return value
directly instead of `ref(path)`. Each new **material** costs shader compile time — prefer a material
*instance* of an existing master wherever the difference is only parameter values.

Runnable: `scripts/create_material.txt`.

## Create a material instance

```python
ip = "/Game/Materials/MI_PlayerRed"
# Arg order is folderPath, assetName, parent — not the old (parent, name, folder).
mi = call_tool(tool_name="Create", toolset_name=MIT,
               arguments={"folderPath": "/Game/Materials/", "assetName": "MI_PlayerRed",
                          "parent": ref("/Game/Materials/M_Character")})["returnValue"]

call_tool(tool_name="SetVectorParameter", toolset_name=MIT,
          arguments={"instance": mi, "name": "BaseColor",
                     "value": {"r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0}})
call_tool(tool_name="SetScalarParameter", toolset_name=MIT,
          arguments={"instance": mi, "name": "Roughness", "value": 0.3})
call_tool(tool_name="SaveAsset", toolset_name=AT, arguments={"assetPath": ip})
```

- `ListParameters` on the **parent** tells you what you may override, each entry with a `name` and a
  `type` (`Scalar` / `Vector` / `Texture` / `StaticSwitch`). Setting a name that is not a parameter of
  that type raises.
- Getters return the **effective** value, inheriting from the parent when not overridden. Use
  `SetParameterOverride(instance, name, bOverride)` to toggle an override without changing the value,
  `ClearParameters` to revert everything, and `SetParent` to re-point the instance.
- Overriding a **StaticSwitch** triggers a shader recompile; the other three types do not.

Runnable: `scripts/create_instance.txt`. (For per-actor tiling on Megascans surfaces, prefer a child
MI with the `Tiling` scalar overridden — see SKILL.md Critical Rules.)

## Add a texture parameter

```python
tex = add(path, "MaterialExpressionTextureSampleParameter2D", -500, 0)
set_props(tex, {"ParameterName": "DiffuseMap", "Group": "Textures",
                "Texture": ref("/Game/Textures/T_Diffuse"),
                "SamplerType": "SAMPLERTYPE_Color"})        # ← data/ORM maps need SAMPLERTYPE_Masks
call_tool(tool_name="ConnectToOutput", toolset_name=MT,
          arguments={"expression": tex, "outputName": "", "materialProperty": "MP_BaseColor"})
call_tool(tool_name="Recompile", toolset_name=MT, arguments={"materialOrFunction": ref(path)})
```

`SamplerType` must match the texture's content or `Recompile` raises. `SAMPLERTYPE_Color` for sRGB
albedo, `SAMPLERTYPE_Normal` for tangent normals, `SAMPLERTYPE_Masks` or `SAMPLERTYPE_LinearColor`
for ORM / packed data, `SAMPLERTYPE_LinearGrayscale` for a single linear channel.

## Create a math expression

```python
color = add(path, "MaterialExpressionVectorParameter", -600, 0)
set_props(color, {"ParameterName": "TintColor", "Group": "Surface"})
inten = add(path, "MaterialExpressionScalarParameter", -600, 150)
set_props(inten, {"ParameterName": "Intensity", "Group": "Surface", "DefaultValue": 1.0})

mult = add(path, "MaterialExpressionMultiply", -300, 50)
for src, pin in ((color, "A"), (inten, "B")):
    call_tool(tool_name="ConnectExpressions", toolset_name=MT,
              arguments={"fromExpression": src, "fromOutputName": "",
                         "toExpression": mult, "toInputName": pin})
call_tool(tool_name="ConnectToOutput", toolset_name=MT,
          arguments={"expression": mult, "outputName": "", "materialProperty": "MP_BaseColor"})
call_tool(tool_name="Recompile", toolset_name=MT, arguments={"materialOrFunction": ref(path)})
```

Unsure of a pin name? `GetExpressionInputNames(expression)` returns exactly the names
`ConnectExpressions` accepts, and a connect that does not land raises rather than wiring nothing.
Unsure of a class path? `ListExpressionClasses(materialOrFunction, search="Multiply")`.

## Set material properties

```python
set_props(ref(path), {"BlendMode": "BLEND_Translucent", "ShadingModel": "MSM_DefaultLit",
                      "TwoSided": True})
call_tool(tool_name="Recompile", toolset_name=MT, arguments={"materialOrFunction": ref(path)})
```

Internal names only — the display-name tolerance went with `MaterialService`. Find a property with
`ObjectToolset.ListProperties` (`{"instance": ref(path), "nameFilter": "Blend", "maxDepth": 0}`) and
read its type and legal values with `GetPropertySchema`.

## Function call node (MaterialFunction)

**VibeUE, through `execute_python`.** Epic's `AddExpression` + `SetProperties({"MaterialFunction": ...})`
produces a node with **no pins**, because the pins are built by `SetMaterialFunctionEx` →
`UpdateFromFunctionResource` inside `PostEditChangeProperty`, which the generic property writer never
calls. `create_function_call` is the only working path.

```python
fn = unreal.MaterialNodeService.create_function_call(path,
    "/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals", -600, 0)
fn_ref = {"refPath": fn.object_path}      # hand the node to Epic calls as a refPath

call_tool(tool_name="ConnectToOutput", toolset_name=MT,
          arguments={"expression": fn_ref, "outputName": "", "materialProperty": "MP_Normal"})
call_tool(tool_name="Recompile", toolset_name=MT, arguments={"materialOrFunction": ref(path)})
```

The node's pin names are on the returned struct (`fn.input_names` / `fn.output_names`), and
`GetExpressionInputNames(fn_ref)` reports the same bare names — which is what `toInputName` takes.

## Custom HLSL expression

Epic's generic path, because a Custom node's pins come from its own `Inputs` array:

```python
custom = add(path, "MaterialExpressionCustom", -500, 0)
set_props(custom, {"Code": "return sin(Time * Speed);", "Description": "SineWave",
                   "OutputType": "CMOT_Float1",     # CMOT_Float1..4, CMOT_MaterialAttributes
                   "Inputs": [{"InputName": "Time"}, {"InputName": "Speed"}]})
call_tool(tool_name="ConnectToOutput", toolset_name=MT,
          arguments={"expression": custom, "outputName": "", "materialProperty": "MP_EmissiveColor"})
call_tool(tool_name="Recompile", toolset_name=MT, arguments={"materialOrFunction": ref(path)})
```

`Inputs` is a container: set the **whole array** to add or remove pins. Extra outputs go in
`AdditionalOutputs` (`[{"OutputName": "Mask", "OutputType": "CMOT_Float3"}]`); reach them by name as
`fromOutputName`.

## Collection parameter

**VibeUE, through `execute_python`** — same reason as the function call: the node's `ParameterId`
GUID is looked up in the collection inside `PostEditChangeProperty`, and without it the material
compiles to `"CollectionParameter has invalid parameter"`.

```python
coll = unreal.MaterialNodeService.create_collection_parameter(
    path, "/Game/Materials/MPC_GlobalParams", "WindStrength", -500, 0)
call_tool(tool_name="ConnectExpressions", toolset_name=MT,
          arguments={"fromExpression": {"refPath": coll.object_path}, "fromOutputName": "",
                     "toExpression": mult, "toInputName": "B"})
```

Need the collection asset itself? `MaterialToolset.CreateParameterCollection(folderPath, assetName)`,
then `ObjectToolset.SetProperties` for its `ScalarParameters` / `VectorParameters` arrays.

## Static switch / texture object parameter

```python
sw = add(path, "MaterialExpressionStaticSwitchParameter", -600, 0)
set_props(sw, {"ParameterName": "UseDetailTexture", "Group": "Features", "DefaultValue": True})
# its input pins are named True and False

to = add(path, "MaterialExpressionTextureObjectParameter", -500, 0)
set_props(to, {"ParameterName": "DetailMap", "Group": "Textures",
               "Texture": ref("/Game/Textures/T_Detail"), "SamplerType": "SAMPLERTYPE_Color"})
```

Use a `TextureObjectParameter` (not a `TextureSampleParameter2D`) to feed a function's
`TextureObject` input — the function brings its own sampler.

## Build a graph

There is no batch API any more: `AddExpression` and `ConnectExpressions` are one node and one wire
per call. Drive them from a table so a graph is still one block of code, and keep the handles the
calls return — they are the `{refPath}` you wire with.

```python
specs = [("MaterialExpressionMultiply", -400, 0), ("MaterialExpressionAdd", -400, 200),
         ("MaterialExpressionLinearInterpolate", -200, 100), ("MaterialExpressionOneMinus", -600, 0)]
nodes = [add(path, cls, x, y) for cls, x, y in specs]

wires = [(nodes[0], "", nodes[2], "A"), (nodes[1], "", nodes[2], "B"), (nodes[3], "", nodes[0], "A")]
for src, out, dst, pin in wires:
    call_tool(tool_name="ConnectExpressions", toolset_name=MT,
              arguments={"fromExpression": src, "fromOutputName": out,
                         "toExpression": dst, "toInputName": pin})
call_tool(tool_name="ConnectToOutput", toolset_name=MT,
          arguments={"expression": nodes[2], "outputName": "", "materialProperty": "MP_BaseColor"})
```

Runnable: `scripts/material_graph_batch.txt`.

## Tidy and verify

```python
call_tool(tool_name="DeleteUnusedExpressions", toolset_name=MT, arguments={"material": ref(path)})
call_tool(tool_name="LayoutExpressions", toolset_name=MT, arguments={"materialOrFunction": ref(path)})
call_tool(tool_name="Recompile", toolset_name=MT, arguments={"materialOrFunction": ref(path)})
stats = call_tool(tool_name="GetStatistics", toolset_name=MT,
                  arguments={"material": ref(path)})["returnValue"]
print(stats["num_pixel_texture_samples"], stats["num_samplers"],
      stats["num_pixel_shader_instructions"])
call_tool(tool_name="SaveAsset", toolset_name=AT, arguments={"assetPath": path})
```

`DeleteUnusedExpressions` does not recompile — always `Recompile` after it. `Recompile` raises with
the failing node's object path on a compile error; a clean compile returns quietly.
`LayoutExpressions` auto-arranges the whole graph, so approximate `x`/`y` at creation is fine.
