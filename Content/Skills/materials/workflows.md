---
name: materials/workflows
description: Step-by-step material authoring workflows — create a material, material instance, texture/math/function-call/custom-HLSL/collection/static-switch nodes, set material properties, and batch create/connect/set node operations.
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
- Batch create / connect / set

VibeUE is an extension on Unreal's native MCP endpoint. **Material/function BASICS** (create
material, add expression, connect, set instance parameters) go through the **engine** material
toolsets via `call_tool(...)` — `editor_toolset.toolsets.material.MaterialTools` for
materials/functions, `editor_toolset.toolsets.material_instance.MaterialInstanceTools` for
instances. The **graph/parameter authoring DELTA** (`create_parameter`, `create_function_call`,
`create_custom_expression`, `batch_*`, diagnostics, export) stays on
`unreal.MaterialNodeService` via `execute_python_code`.

Surviving create calls return result objects — read `.id` (MaterialExpressionInfo from
`create_parameter`/`create_function_call`), never the raw object. Compile after graph changes.
Runnable examples in `scripts/`.

## Create a basic material

```python
import unreal
ms, mns = unreal.MaterialService, unreal.MaterialNodeService

# Engine: create the material asset (returns the Material object, no .asset_path).
call_tool(
    tool_name="create_material",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"folder_path": "/Game/Materials/", "asset_name": "M_Character"},
)
path = "/Game/Materials/M_Character"

# VibeUE delta: parameter authoring stays on MaterialNodeService.
color = mns.create_parameter(path, "Vector", "BaseColor", "Surface", "0.8,0.2,0.2,1.0", -500, 0)
# Engine: connect an expression output to a material property.
call_tool(
    tool_name="connect_to_output",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"expression": color.id, "output_name": "", "material_property": "MP_BaseColor"},
)
rough = mns.create_parameter(path, "Scalar", "Roughness", "Surface", "0.5", -500, 100)
call_tool(
    tool_name="connect_to_output",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"expression": rough.id, "output_name": "", "material_property": "MP_Roughness"},
)
ms.compile_material(path)
unreal.EditorAssetLibrary.save_asset(path)
mns.layout_expressions(path)   # auto-arrange into clean columns
```
Runnable: `scripts/create_material.txt`.

## Create a material instance

```python
import unreal
ms = unreal.MaterialService

# Engine: create the instance, then set its overrides — all via MaterialInstanceTools.
call_tool(
    tool_name="create_material_instance",
    toolset_name="editor_toolset.toolsets.material_instance.MaterialInstanceTools",
    arguments={"parent_material": "/Game/Materials/M_Character",
               "asset_name": "MI_PlayerRed", "folder_path": "/Game/Materials/"},
)
ip = "/Game/Materials/MI_PlayerRed"
call_tool(
    tool_name="set_vector_parameter",
    toolset_name="editor_toolset.toolsets.material_instance.MaterialInstanceTools",
    arguments={"instance_path": ip, "parameter_name": "BaseColor", "r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0},
)
call_tool(
    tool_name="set_scalar_parameter",
    toolset_name="editor_toolset.toolsets.material_instance.MaterialInstanceTools",
    arguments={"instance_path": ip, "parameter_name": "Roughness", "value": 0.3},
)
unreal.EditorAssetLibrary.save_asset(ip)
```
Runnable: `scripts/create_instance.txt`. (For per-actor tiling on Megascans surfaces, prefer a child MI
with the `Tiling` scalar overridden — see SKILL.md Critical Rules.) For setting many instance
parameters at once, VibeUE keeps `ms.set_instance_parameters_bulk(...)`.

## Add a texture parameter

```python
tex = mns.create_parameter(path, "Texture", "DiffuseMap", "Textures", "", -500, 0)
# Engine: connect the parameter output to a material property.
call_tool(
    tool_name="connect_to_output",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"expression": tex.id, "output_name": "", "material_property": "MP_BaseColor"},
)
ms.compile_material(path)
```

## Create a math expression

```python
color = mns.create_parameter(path, "Vector", "TintColor", "Surface", "", -600, 0)
inten = mns.create_parameter(path, "Scalar", "Intensity", "Surface", "1.0", -600, 100)
MT = "editor_toolset.toolsets.material.MaterialTools"
# Engine: add a generic expression node, then wire it.
mult = call_tool(tool_name="add_expression", toolset_name=MT,
                 arguments={"material_or_function": path, "expression_class": "Multiply",
                            "x": -300, "y": 0})
mult_id = mult.id
call_tool(tool_name="connect_expressions", toolset_name=MT,
          arguments={"from_expression": color.id, "from_output_name": "",
                     "to_expression": mult_id, "to_input_name": "A"})
call_tool(tool_name="connect_expressions", toolset_name=MT,
          arguments={"from_expression": inten.id, "from_output_name": "",
                     "to_expression": mult_id, "to_input_name": "B"})
call_tool(tool_name="connect_to_output", toolset_name=MT,
          arguments={"expression": mult_id, "output_name": "", "material_property": "MP_BaseColor"})
ms.compile_material(path)
```

## Set material properties

```python
ms.set_property(path, "BlendMode", "BLEND_Translucent")  # display "Blend Mode" also works
ms.set_property(path, "ShadingModel", "MSM_DefaultLit")
ms.set_property(path, "TwoSided", "true")
ms.compile_material(path)
```

## Function call node (MaterialFunction)

```python
fn = mns.create_function_call(path,
    "/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals", -600, 0)
call_tool(
    tool_name="connect_to_output",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"expression": fn.id, "output_name": "", "material_property": "MP_Normal"},
)
ms.compile_material(path)
```
Input names for function calls are the **bare** names from `get_function_info(func_path).inputs`
(never the display labels with type suffixes like `"TextureObject (T2d)"`).

## Custom HLSL expression

```python
custom = mns.create_custom_expression(path, "return sin(Time * Speed);", "SineWave",
    "Float1", "Time,Speed", -500, 0)   # OutputType: Float1..Float4, MaterialAttributes
call_tool(
    tool_name="connect_to_output",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"expression": custom.id, "output_name": "", "material_property": "MP_EmissiveColor"},
)
ms.compile_material(path)
```

## Collection parameter

```python
coll = mns.create_collection_parameter(path, "/Game/Materials/MPC_GlobalParams", "WindStrength", -500, 0)
# Engine: wire the collection-parameter output into another node's input.
call_tool(
    tool_name="connect_expressions",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"from_expression": coll.id, "from_output_name": "",
               "to_expression": some_mult_id, "to_input_name": "B"},
)
```

## Static switch / texture object parameter

```python
sw = mns.create_parameter(path, "StaticSwitch", "UseDetailTexture", "Features", "true", -600, 0)
to = mns.create_parameter(path, "TextureObject", "DetailMap", "Textures", "/Game/Textures/T_Detail.T_Detail", -500, 0)
```

## Batch create / connect / set

Each batch call applies in one transaction (much faster than per-node). Arrays must be equal length.

```python
types = ["Multiply", "Add", "Lerp", "OneMinus"]
names = ["Mult1", "Add1", "Lerp1", "Invert1"]   # "" to skip
xs, ys = [-400, -400, -200, -600], [0, 200, 100, 0]
created = mns.batch_create_expressions(path, types, names, xs, ys)

mns.batch_connect_expressions(path, source_ids, output_names, target_ids, input_names)
mns.batch_set_properties(path, node_ids, property_names, property_values)

# Wiring a single node into a material output uses the engine MaterialTools call:
call_tool(
    tool_name="connect_to_output",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"expression": created[0].id, "output_name": "", "material_property": "MP_BaseColor"},
)
```
Runnable: `scripts/material_graph_batch.txt`.
