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

All create calls return result objects — read `.asset_path` (MaterialCreateResult) or `.id`
(MaterialExpressionInfo), never the raw object. Compile after graph changes. Runnable examples in `scripts/`.

## Create a basic material

```python
import unreal
ms, mns = unreal.MaterialService, unreal.MaterialNodeService

result = ms.create_material("M_Character", "/Game/Materials/")
path = result.asset_path
color = mns.create_parameter(path, "Vector", "BaseColor", "Surface", "0.8,0.2,0.2,1.0", -500, 0)
mns.connect_to_output(path, color.id, "", "BaseColor")
rough = mns.create_parameter(path, "Scalar", "Roughness", "Surface", "0.5", -500, 100)
mns.connect_to_output(path, rough.id, "", "Roughness")
ms.compile_material(path)
unreal.EditorAssetLibrary.save_asset(path)
mns.layout_expressions(path)   # auto-arrange into clean columns
```
Runnable: `scripts/create_material.pyx`.

## Create a material instance

```python
import unreal
ms = unreal.MaterialService
result = ms.create_instance("/Game/Materials/M_Character", "MI_PlayerRed", "/Game/Materials/")
ip = result.asset_path
ms.set_instance_vector_parameter(ip, "BaseColor", 1.0, 0.0, 0.0, 1.0)
ms.set_instance_scalar_parameter(ip, "Roughness", 0.3)
unreal.EditorAssetLibrary.save_asset(ip)
```
Runnable: `scripts/create_instance.pyx`. (For per-actor tiling on Megascans surfaces, prefer a child MI
with the `Tiling` scalar overridden — see SKILL.md Critical Rules.)

## Add a texture parameter

```python
tex = mns.create_parameter(path, "Texture", "DiffuseMap", "Textures", "", -500, 0)
mns.connect_to_output(path, tex.id, "", "BaseColor")
ms.compile_material(path)
```

## Create a math expression

```python
color = mns.create_parameter(path, "Vector", "TintColor", "Surface", "", -600, 0)
mult = mns.create_expression(path, "Multiply", -300, 0)
inten = mns.create_parameter(path, "Scalar", "Intensity", "Surface", "1.0", -600, 100)
mns.connect_expressions(path, color.id, "", mult.id, "A")
mns.connect_expressions(path, inten.id, "", mult.id, "B")
mns.connect_to_output(path, mult.id, "", "BaseColor")
ms.compile_material(path)
```

## Set material properties

```python
ms.set_blend_mode(path, "Translucent")   # enum accepts "Translucent" or "BLEND_Translucent"
ms.set_shading_model(path, "DefaultLit")
ms.set_two_sided(path, True)
ms.compile_material(path)
```

## Function call node (MaterialFunction)

```python
fn = mns.create_function_call(path,
    "/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals", -600, 0)
mns.connect_to_output(path, fn.id, "", "Normal")
ms.compile_material(path)
```
Input names for function calls are the **bare** names from `get_function_info(func_path).inputs`
(never the display labels with type suffixes like `"TextureObject (T2d)"`).

## Custom HLSL expression

```python
custom = mns.create_custom_expression(path, "return sin(Time * Speed);", "SineWave",
    "Float1", "Time,Speed", -500, 0)   # OutputType: Float1..Float4, MaterialAttributes
mns.connect_to_output(path, custom.id, "", "EmissiveColor")
ms.compile_material(path)
```

## Collection parameter

```python
coll = mns.create_collection_parameter(path, "/Game/Materials/MPC_GlobalParams", "WindStrength", -500, 0)
mns.connect_expressions(path, coll.id, "", some_mult_id, "B")
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
```
Runnable: `scripts/material_graph_batch.pyx`.
