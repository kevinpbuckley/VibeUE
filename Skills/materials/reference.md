---
name: materials/reference
description: Material reference for UE 5.9 — the full MaterialToolset / MaterialInstanceToolset / ObjectToolset tool inventory, the two surviving VibeUE MaterialNodeService calls, parameter expression classes, material output names, enum formats, reading a graph, duplicate-and-retarget, and Material Function authoring.
---

# Material Reference

## Contents
- Who owns what
- MaterialToolset (25)
- MaterialInstanceToolset (14)
- Properties: ObjectToolset
- Assets: AssetToolset
- Parameter expression classes
- Material output properties
- Enum value formats
- Reading a graph
- Duplicate and retarget
- Material Functions
- Removed tools → replacement

## Who owns what

`UMaterialService` was deleted (16 tools). `UMaterialNodeService` is down to **two** calls. The rest
is Epic's `EditorToolset`, via `call_tool`.

| Reached with | Namespace |
|---|---|
| `call_tool` | `EditorToolset.MaterialToolset`, `EditorToolset.MaterialInstanceToolset`, `EditorToolset.ObjectToolset`, `EditorToolset.AssetToolset`, `EditorToolset.EditorAppToolset` |
| `execute_python` | `unreal.MaterialNodeService.create_function_call`, `unreal.MaterialNodeService.create_collection_parameter` |

Argument conventions for every Epic call: lowerCamelCase arg names, object/class arguments as
`{"refPath": "/Game/Dir/Asset.Asset"}` handles, enums as their prefixed identifier strings. Results
arrive as `{"returnValue": ...}`; object returns are themselves `{refPath}` handles you can pass
straight back in.

## MaterialToolset (25)

| Tool | Args | Notes |
|---|---|---|
| `CreateMaterial` | `folderPath`, `assetName` | returns the Material handle. Each new material costs compile time |
| `CreateFunction` | `folderPath`, `assetName` | new MaterialFunction |
| `CreateParameterCollection` | `folderPath`, `assetName` | new MPC |
| `ListExpressionClasses` | `materialOrFunction`, `search` | full class paths; `FunctionInput`/`FunctionOutput` only for a function |
| `AddExpression` | `materialOrFunction`, `expressionClass`, `x`, `y` | `expressionClass` is a class **refPath**, not a bare name |
| `DeleteExpression` | `materialOrFunction`, `expression` | |
| `DeleteUnusedExpressions` | `material` | sweeps nodes reaching no output; does **not** recompile |
| `GetExpressions` | `materialOrFunction` | every node, as handles |
| `GetExpressionInputNames` | `expression` | the exact names `ConnectExpressions` accepts |
| `GetExpressionOutputNames` | `expression` | `""` = default (first) output |
| `GetExpressionInputs` | `materialOrFunction`, `expression` | live wiring per input pin; `expression: null` = unwired |
| `ConnectExpressions` | `fromExpression`, `fromOutputName`, `toExpression`, `toInputName` | **raises** if the connection does not land |
| `DisconnectExpressions` | `toExpression`, `toInputName` | `""` disconnects the first input |
| `GetPropertyInput` | `material`, `materialProperty` | what drives an output today |
| `ConnectToOutput` | `expression`, `outputName`, `materialProperty` | raises on failure |
| `DisconnectFromOutput` | `material`, `materialProperty` | |
| `LayoutExpressions` | `materialOrFunction` | auto-arrange the whole graph |
| `ListParameterGroups` | `materialOrFunction` | `""` = ungrouped |
| `RenameParameterGroup` | `materialOrFunction`, `oldName`, `newName` | merges into an existing name; recompiles internally |
| `DeleteParameterGroup` | `materialOrFunction`, `groupName` | ungroups, keeps the parameters; recompiles internally |
| `Recompile` | `materialOrFunction` | **raises** node-localized compile errors |
| `GetStatistics` | `material` | post-compile shader-map counts (takes a MaterialInterface, so instances too) |
| `GetReferencingMaterials` | `materialFunction` | asset data for every material using the function |
| `DiffMaterial` | `oldAssetPath`, `newAssetPath` | readable text diff; empty when identical |
| `DiffFunction` | `oldAssetPath`, `newAssetPath` | same, for functions |

`GetStatistics` keys: `num_pixel_shader_instructions`, `num_vertex_shader_instructions`,
`num_samplers`, `num_pixel_texture_samples`, `num_vertex_texture_samples`,
`num_virtual_texture_samples`, `num_uv_scalars`, `num_interpolator_scalars`.

`Recompile` on a **function** also recompiles every material referencing it. `RenameParameterGroup`
and `DeleteParameterGroup` recompile themselves — no extra `Recompile` needed.

## MaterialInstanceToolset (14)

| Tool | Args |
|---|---|
| `Create` | `folderPath`, `assetName`, `parent` — **note the order** |
| `ListParameters` | `material` → entries of `{name, type}` with type `Scalar`/`Vector`/`Texture`/`StaticSwitch` |
| `GetScalarParameter` / `SetScalarParameter` | `instance`, `name` (+ `value`: float) |
| `GetVectorParameter` / `SetVectorParameter` | `instance`, `name` (+ `value`: `{r,g,b,a}`; >1 is valid for HDR/emissive) |
| `GetTextureParameter` / `SetTextureParameter` | `instance`, `name` (+ `value`: texture refPath) |
| `GetStaticSwitchParameter` / `SetStaticSwitchParameter` | `instance`, `name` (+ `value`: bool) |
| `SetParent` | `instance`, `parent` |
| `ClearParameters` | `instance` — revert every override to the parent |
| `SetParameterOverride` | `instance`, `name`, `bOverride` |
| `Diff` | `oldAssetPath`, `newAssetPath` |

- Getters return the **effective** value, inherited from the parent when not overridden. A name that
  is not a parameter of that type raises.
- Overriding a StaticSwitch recompiles shaders; the other three types do not.
- `SetParameterOverride(..., False)` discards the stored value for non-static types — re-enabling
  later restores the *parent* value, not your old override. Static switches and static component
  masks keep their value across the toggle.

## Properties: ObjectToolset

`EditorToolset.ObjectToolset` is the only property reader/writer left, for materials, instances and
expressions alike.

| Tool | Args |
|---|---|
| `ListProperties` | `instance`, `root`, `nameFilter` (regex, case-insensitive), `maxDepth` (default 1; `0` = every level) |
| `GetPropertySchema` | `instance`, `root`, `maxDepth` — types and descriptions; much larger than `ListProperties` |
| `GetProperties` | `instance`, `properties` (array of names or dotted paths) → JSON string |
| `SetProperties` | `instance`, `values` (**a JSON string**) → bool |
| `ResetProperties` | `instance`, `properties` — back to class defaults |

Find the property with `ListProperties`, learn its type from `GetPropertySchema`, then write it.
`SetProperties` returns a bool — check it. A container element is addressed by subscript
(`"Inputs[0].InputName"`) only when it already exists; set the whole array to add or remove
elements. A value outside the bounds the schema advertises is refused.

## Assets: AssetToolset

`Exists(path)`, `SaveAsset(assetPath)`, `Duplicate(path, newPath)`, `Delete(path)`,
`GetReferencers(assetPath)`, `GetDependencies(assetPath)`. `SaveAsset` is what persists a material
after `Recompile`. For a material that may still be referenced,
`unreal.AssetDiscoveryService.delete_asset_unattended` names the holder instead of opening a modal
dialog — check `GetReferencers` first either way.

## Parameter expression classes

`AddExpression` the class, then `ObjectToolset.SetProperties` the identity and default. Class
refPaths are `/Script/Engine.` + the name below.

| Parameter | Class | Properties |
|---|---|---|
| Scalar | `MaterialExpressionScalarParameter` | `ParameterName`, `Group`, `SortPriority`, `DefaultValue` (float) |
| Vector / colour | `MaterialExpressionVectorParameter` | `ParameterName`, `Group`, `SortPriority`, `DefaultValue` (`{r,g,b,a}`) |
| Texture (with sampler) | `MaterialExpressionTextureSampleParameter2D` | `ParameterName`, `Group`, `Texture` (refPath), `SamplerType` |
| Texture object (no sampler) | `MaterialExpressionTextureObjectParameter` | `ParameterName`, `Group`, `Texture` (refPath), `SamplerType` |
| Static switch | `MaterialExpressionStaticSwitchParameter` | `ParameterName`, `Group`, `DefaultValue` (bool); input pins are `True` / `False` |
| Collection parameter | — | **`unreal.MaterialNodeService.create_collection_parameter`** |
| Material function call | — | **`unreal.MaterialNodeService.create_function_call`** |

`SamplerType` values: `SAMPLERTYPE_Color` (sRGB albedo), `SAMPLERTYPE_LinearColor`,
`SAMPLERTYPE_Normal` (tangent normals), `SAMPLERTYPE_Masks` (ORM / packed data),
`SAMPLERTYPE_Grayscale`, `SAMPLERTYPE_LinearGrayscale`. A mismatch makes `Recompile` raise
`"Sampler type is Color, should be Masks for /Game/.../T_X_ORM"`.

Custom HLSL is `MaterialExpressionCustom` with properties `Code`, `OutputType`
(`CMOT_Float1`..`CMOT_Float4`, `CMOT_MaterialAttributes`), `Description`,
`Inputs` (`[{"InputName": "Time"}, ...]`), `AdditionalOutputs`
(`[{"OutputName": "Mask", "OutputType": "CMOT_Float3"}]`), `AdditionalDefines`, `IncludeFilePaths`.

## Material output properties

`materialProperty` is the `EMaterialProperty` identifier, not a friendly name:
`MP_BaseColor`, `MP_Metallic`, `MP_Specular`, `MP_Roughness`, `MP_EmissiveColor`, `MP_Normal`,
`MP_Opacity`, `MP_OpacityMask`, `MP_WorldPositionOffset`, `MP_AmbientOcclusion`.

## Enum value formats

Through `ObjectToolset.SetProperties`, enum values are the full prefixed identifiers:
`BLEND_Opaque` / `BLEND_Masked` / `BLEND_Translucent` / `BLEND_Additive`, `MSM_DefaultLit`,
`MD_Surface`, `SAMPLERTYPE_*`, `CMOT_*`. Property **names** are the internal ones (`TwoSided`,
`BlendMode`, `ShadingModel`, `MaterialDomain`, `OpacityMaskClipValue`, `bUsedWithStaticLighting`,
`DitheredLODTransition`) — the display-name tolerance went with `MaterialService`. Read legal values
from `GetPropertySchema` rather than guessing.

## Reading a graph

There is no `export_material_graph`. The equivalent is three reads, and the result is live handles
rather than JSON ids — which is better, because you can wire with them directly:

1. `GetExpressions(materialOrFunction)` — every node as a `{refPath}` handle.
2. Per node: `ObjectToolset.GetProperties` for `ParameterName` / `Group` / `DefaultValue` /
   class-specific fields, and `GetExpressionInputs(materialOrFunction, expression)` for its wiring
   (`inputName`, `expression`, `outputName`; `expression: null` = unwired).
3. Per output: `GetPropertyInput(material, materialProperty)` for what drives it.

A node's class is in its `refPath` (`.../M_X.M_X:MaterialExpressionMultiply_0`), or ask
`ObjectToolset.GetClass`. See `scripts/export_graph.txt`.

## Duplicate and retarget

The old "recreate from export" dance (export JSON → batch-create → remap ids → reconnect) is gone
with the batch API. Cloning a material is now one call plus edits on the copy:

1. `AssetToolset.Duplicate(path, newPath)`.
2. `ObjectToolset.SetProperties` on the copy for `BlendMode` / `ShadingModel` / etc.
3. `GetExpressions` on the copy, then `SetProperties` per node to retarget textures, parameter names
   and defaults.
4. `AddExpression` / `DeleteExpression` / `ConnectExpressions` for structural changes;
   `DeleteUnusedExpressions` to sweep what you orphaned.
5. `LayoutExpressions`, `Recompile`, `AssetToolset.SaveAsset`.
6. `DiffMaterial(path, newPath)` to confirm the copy differs only where you intended.

Keep parameter **names** identical if existing material instances must keep their overrides — an
override is keyed by name, and a rename silently drops it back to the default.

## Material Functions

`MaterialToolset` treats a MaterialFunction as a graph just like a material: `CreateFunction`,
`AddExpression`, `ConnectExpressions`, `GetExpressions`, `LayoutExpressions`, `Recompile` (which also
recompiles dependent materials), `ListParameterGroups`, `DiffFunction`.

- I/O pins are expressions: `AddExpression` of
  `/Script/Engine.MaterialExpressionFunctionInput` / `MaterialExpressionFunctionOutput`, then
  `ObjectToolset.SetProperties` for `InputName` / `OutputName`, `InputType`
  (`FunctionInput_Scalar`, `FunctionInput_Vector2`..`Vector4`, `FunctionInput_Texture2D`, ...),
  `SortPriority`, `bUsePreviewValueAsDefault`, `PreviewValue`. These two classes appear in
  `ListExpressionClasses` only for a function, never for a material.
- Library exposure, description and categories are properties on the function asset:
  `SetProperties` with `bExposeToLibrary`, `Description`, `LibraryCategoriesText`.
- `GetReferencingMaterials(materialFunction)` lists every material that will recompile when you
  change it — check it before editing a shared function.
- Referencing a function from a material graph is **`unreal.MaterialNodeService.create_function_call`**,
  not `AddExpression` (see SKILL.md for why), and its input pin names are the bare names reported by
  `GetExpressionInputNames` on the created node.

For landscape-specific function patterns, load `landscape-auto-material`.

## Removed tools → replacement

Everything below was removed on the 5.9 branch. If you find a call to one in older code or notes,
this is what it becomes.

| Removed | Use instead |
|---|---|
| all 16 `MaterialService` tools (`create_material`, `create_material_instance`, `compile_material`, `save_material`, `set_property`/`get_property`/`list_properties`/`get_property_info`, `get_material_info`/`get_material_info_result`, `summarize`, `list_parameters`, `set_instance_parameters_bulk`, …) | `MaterialToolset.CreateMaterial` / `Recompile`, `MaterialInstanceToolset.Create` / `ListParameters` / `Set*Parameter`, `ObjectToolset.*`, `AssetToolset.SaveAsset` |
| `create_parameter`, `promote_to_parameter`, `set_parameter_metadata` | `AddExpression` of the parameter class + `ObjectToolset.SetProperties` |
| `create_custom_expression` | `AddExpression` of `MaterialExpressionCustom` + `SetProperties` |
| `batch_create_expressions`, `batch_create_specialized`, `batch_connect_expressions`, `batch_set_properties` | loop `AddExpression` / `ConnectExpressions` / `SetProperties` (see workflows.md → Build a graph) |
| `connect_expression_to_output`, `disconnect_output` | `MaterialToolset.ConnectToOutput` / `DisconnectFromOutput` (`MP_` enum names) |
| `get_expression_pins`, `get_expression_details`, `list_expressions`, `list_connections` | `GetExpressionInputNames` / `GetExpressionOutputNames` / `GetExpressionInputs` / `GetExpressions`, `ObjectToolset.GetProperties` |
| `get_property`/`set_property`/`list_properties` on expressions | `ObjectToolset.GetProperties` / `SetProperties` / `ListProperties` |
| `discover_types`, `get_categories` | `MaterialToolset.ListExpressionClasses` |
| `layout_expressions` | `MaterialToolset.LayoutExpressions` (it exists — see SKILL.md) |
| `get_material_diagnostics` | `MaterialToolset.Recompile` (raises, node-localized) + `GetStatistics` |
| `export_material_graph`, `export_graph_summary`, `compare_graphs` | `GetExpressions` + `GetExpressionInputs` + `GetPropertyInput`; `DiffMaterial` for comparison |
| `get_output_properties`, `get_output_connections` | the `MP_*` list above; `GetPropertyInput` per property |
| `delete_material_expression`, `move` | `MaterialToolset.DeleteExpression`; set `MaterialExpressionEditorX`/`Y` via `ObjectToolset`, or just `LayoutExpressions` |
| `get_function_info`, `export_function_graph`, `create_material_function`, `add_function_input`/`add_function_output` | `MaterialToolset.CreateFunction`, `GetExpressions`, `GetExpressionInputNames`, `AddExpression` of `FunctionInput`/`FunctionOutput` + `SetProperties` |
| `create_function_call`, `create_collection_parameter` | **not removed** — still `unreal.MaterialNodeService`, via `execute_python` |
