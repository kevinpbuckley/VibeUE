---
name: materials
display_name: Material System
description: Create and edit materials and material instances — graph nodes, parameters, functions, custom HLSL, and instance overrides (MaterialService + MaterialNodeService). Use when the user asks to create or edit a material/material instance, wire material nodes, add material parameters, set blend/shading modes, or recreate a material graph. For landscape materials load landscape-materials.
vibeue_classes:
  - MaterialService
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

## Critical Rules

### 🚨 Connecting Function Call Inputs: Use Bare Names, Not Display Names

`get_expression_pins(mat, expr_id)` returns input pins with **display labels including type suffixes** like `"TextureObject (T2d)"`, `"TextureSize (V3)"`, `"Use High Quality Normals (SB)"`. **Never pass these display labels to `connect_expressions`** — UE's connect API matches against the *bare* input name only:

```python
# ❌ WRONG — display label with type suffix; connection fails silently or gets ignored
unreal.MaterialNodeService.connect_expressions(mp, src_id, "", fn_id, "TextureObject (T2d)")

# ✅ CORRECT — bare name
unreal.MaterialNodeService.connect_expressions(mp, src_id, "", fn_id, "TextureObject")
```

The authoritative source for input names is `get_function_info(function_path).inputs` — those are the bare names (`TextureObject`, `TextureSize`, `WorldPosition`, etc.). For built-in expressions, use `get_inputs_for_material_expression` from `unreal.MaterialEditingLibrary`.

> ⚠️ Why this is easy to miss: `get_expression_pins` returns `is_connected=True` for both real and phantom connections, and our service used to silently produce phantom wires when given display names. The wires would appear in the graph export but the shader compiler would never see the texture flow through. Always verify with `unreal.MaterialEditingLibrary.get_used_textures(mat)` after wiring — if it returns 0 on a material that samples textures, your connections are phantom.

### 🚨 Verifying Wiring: Use `get_material_diagnostics`

**`MaterialNodeService.get_material_diagnostics(path)` is the canonical way to verify a material is sampling textures and compiling cleanly.** It returns the real compile errors, the textures the shader actually references, and node-type counts. Always run it after non-trivial graph rewiring:

```python
diag = unreal.MaterialNodeService.get_material_diagnostics("/Game/Materials/M_Foo")
assert diag.success
print(f"compiled ok:                     {diag.is_compiled_ok}")
print(f"compile errors ({len(diag.compile_errors)}):")
for e in diag.compile_errors: print(f"  {e}")
print(f"referenced textures ({len(diag.referenced_texture_paths)}):")
for t in diag.referenced_texture_paths: print(f"  {t}")
print(f"expression count:                {diag.expression_count}")
print(f"texture sample count:            {diag.texture_sample_count}")
print(f"texture object parameter count:  {diag.texture_object_parameter_count}")
print(f"function call count:             {diag.function_call_count}")
```

Sample compile errors caught by this:
- `"(Function WorldAlignedTexture) (Node TextureSample) Sampler type is Color, should be Masks for /Game/.../T_xevtfjz_2K_ORM"` — ORM/data textures need `SAMPLERTYPE_Masks` or `SAMPLERTYPE_LinearColor`, not the default `SAMPLERTYPE_Color`.
- Type mismatch errors when wiring scalars to vector inputs without proper conversion.
- Missing required inputs on function calls.

**Why prefer this over the older signals:**

`unreal.MaterialEditingLibrary.get_used_textures(mat)` is **unreliable** for multi-branch graphs (BC + Normal + ORM, or anything with ComponentMask after a function-call output). It returns `0` even when the material is sampling textures correctly. Don't use it as proof of broken wiring.

Visual confirmation via the material editor preview is also useful but harder — see the next section.

```python
# One-shot: opens the asset editor, focuses it, captures.
res = unreal.ScreenshotService.capture_asset_editor(
    "/Game/Materials/M_Foo", "mat_preview")
# Then attach_image(file_path=res.file_path) to inspect.
```

`ScreenshotService.capture_asset_editor` handles the open + focus + capture pipeline in one call.

> ⚠️ Best-effort focus: when many asset editors are already open as tabs, the editor tab may not switch reliably. Close other asset editors first if the screenshot keeps catching the wrong tab:
>
> ```python
> ed = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
> for path in [other_open_paths]:
>     ed.close_all_editors_for_asset(unreal.load_asset(path))
> ```

### 🚨 Tiling on Basic Shapes: Prefer a Child MI with `Tiling` Override

For Megascans `M_MS_Srf` and similar surface materials with a `Tiling` scalar parameter, the cleanest per-actor tiling fix is a **child material instance with `Tiling` overridden** — *not* a UV transform on the mesh. UV transforms apply to all faces uniformly and break orientation; the parent material's UV mapping is already correct per face.

```python
# ✅ CORRECT pattern for "this disc shows brick too sparse / too dense"
r = unreal.MaterialService.create_instance(
    "/Game/Fab/Megascans/.../MI_xevtfjz",
    "MI_xevtfjz_Disc", "/Game/Materials/")
unreal.MaterialService.set_instance_scalar_parameter(r.asset_path, "Tiling", 4.0)
unreal.MaterialService.save_instance(r.asset_path)
# assign r.asset_path to the actor
```

For triplanar / world-aligned (where the same material works on cube, sphere, cylinder with no UV concerns), build a master material wrapping `WorldAlignedTexture` / `WorldAlignedNormal` (functions live under `/Engine/Functions/Engine_MaterialFunctions01/Texturing/`). Use `TextureObjectParameter` (not `TextureSampleParameter2D`) to feed the `TextureObject` input on those functions — the function brings its own sampler. Feed `TextureSize` from a `Constant3Vector` or `Multiply(Scalar, Constant3Vector(1,1,1))`; **a bare scalar will broadcast incorrectly to V3** in this input slot.

### 🚨 Inspect Before Modifying Existing Materials

Before adding, removing, or reconnecting nodes in an **existing** material, you **MUST** export and review the current graph first:

```python
import unreal, json

path = "/Game/Materials/M_Existing"

# Step 1: Get high-level info (blend mode, shading model)
info = unreal.MaterialService.get_material_info(path)
print(f"Blend: {info.blend_mode}, Shading: {info.shading_model}")

# Step 2: Export the full node graph
graph = json.loads(unreal.MaterialNodeService.export_material_graph(path))
print(f"Expressions: {len(graph['expressions'])}")
print(f"Connections: {len(graph['connections'])}")
print(f"Output connections: {len(graph['output_connections'])}")

# Step 3: Review what already exists
for expr in graph['expressions']:
    name = expr.get('parameter_name') or expr.get('class')
    print(f"  [{expr['id']}] {expr['class']} - {name}")
for oc in graph['output_connections']:
    print(f"  Output '{oc['property']}' ← expr {oc['expression_id']}")
```

**Why this matters:**
- The material may already have nodes connected to the output you want to modify
- Blindly adding nodes creates duplicates and orphaned connections
- You need to know what IDs exist so you can reconnect or replace, not just append
- `export_material_graph` returns the ground truth — use it to plan your edits

**Workflow for modifying existing materials:**
1. **Export** the graph JSON and review expressions + connections
2. **Plan** your changes — identify which nodes to keep, replace, or add
3. **Disconnect** existing connections if needed before reconnecting
4. **Add** only the new nodes that don't already exist
5. **Reconnect** outputs to their correct sources
6. **Compile** and save

### 🚨 Return Types — Read `.asset_path` and `.id`, NOT the Raw Return Value

All create methods return **result objects**, not raw strings. Always extract the field you need:

```python
# create_material → MaterialCreateResult
result = unreal.MaterialService.create_material("M_MyMat", "/Game/Materials/")
if not result.success:
    print(f"FAILED: {result.error_message}")
path = result.asset_path  # ← use this, NOT result itself

# create_instance → MaterialCreateResult  (same pattern)
result = unreal.MaterialService.create_instance("/Game/Materials/M_Base", "MI_Red", "/Game/Materials/")
instance_path = result.asset_path

# create_parameter / create_expression / create_function_call → MaterialExpressionInfo
expr = unreal.MaterialNodeService.create_parameter(path, "Vector", "BaseColor", "Surface", "1,0,0,1", -400, 0)
node_id = expr.id  # ← use .id, NOT expr itself

expr2 = unreal.MaterialNodeService.create_expression(path, "Multiply", -200, 0)
mult_id = expr2.id
```

> ⚠️ Passing a result object where a string is expected gives:
> `TypeError: Nativize: Cannot nativize 'MaterialCreateResult' as 'String'`
> `TypeError: Nativize: Cannot nativize 'MaterialExpressionInfo' as 'String'`

### ⚠️ Two Services

- **MaterialService** - Create materials, instances, manage properties
- **MaterialNodeService** - Build material graphs, expressions, parameters

### ⚠️ Compile After Graph Changes

```python
expr = unreal.MaterialNodeService.create_parameter(path, "Vector", "BaseColor", ...)
unreal.MaterialNodeService.connect_to_output(path, expr.id, "", "BaseColor")  # use expr.id
unreal.MaterialService.compile_material(path)  # REQUIRED
unreal.EditorAssetLibrary.save_asset(path)
```

### ⚠️ Parameter Types

| Type | Use | Example Value |
|------|-----|---------------|
| `Scalar` | Single float | `0.5` |
| `Vector` | Color/vector | `(R=1.0,G=0.0,B=0.0,A=1.0)` |
| `Texture` | Texture parameter | `/Game/T_Diffuse.T_Diffuse` |
| `TextureObject` | Texture object (no sampler) | `/Game/T_Diffuse.T_Diffuse` |
| `StaticSwitch` | Static bool switch | `true` or `false` |

### ⚠️ Material Output Names

- `BaseColor`, `Metallic`, `Specular`, `Roughness`
- `EmissiveColor`, `Normal`, `Opacity`, `OpacityMask`
- `WorldPositionOffset`, `AmbientOcclusion`

### ⚠️ Discovering Node Types & Categories

To answer "what nodes / categories are available", use the MaterialNodeService discovery
calls — do **not** hunt through `discover_python_module` or a material-editor palette API
(there isn't a Python one):

```python
# All material-expression categories (Math, Color, Constants, Coordinates, Parameters, ...)
cats = unreal.MaterialNodeService.get_categories()

# Search node types by name/category (returns MaterialExpressionTypeInfo:
#   class_name, display_name, category, description, is_parameter)
types = unreal.MaterialNodeService.discover_types(category="", search_term="Add", max_results=100)
```

To create a node, pass the bare class name minus the `MaterialExpression` prefix
(`"Add"`, `"Multiply"`, `"Constant3Vector"`, `"OneMinus"`, `"LinearInterpolate"`/`"Lerp"`) to
`create_expression` / `batch_create_expressions`.

### ⚠️ Check Node Existence

```python
# WRONG - crashes if not found
add_node = next((n for n in nodes if "Add" in n.display_name))

# CORRECT
add_node = next((n for n in nodes if "Add" in n.display_name), None)
if add_node:
    pins = unreal.MaterialNodeService.get_expression_pins(mat_path, add_node.id)
```

### ⚠️ Property Names

Use `discover_python_class()` first (batch: `class_name='unreal.A, unreal.B'`):
- `MaterialExpressionTypeInfo` uses `display_name`, NOT `name`
- `MaterialOutputConnectionInfo` uses `connected_expression_id`, NOT `expression_id`

Complete field lists (don't guess):

| Struct | Fields |
|---|---|
| `MaterialExpressionInfo` (from `list_expressions` / `get_expression_info`) | `id`, `class_name`, `display_name`, `category`, `description`, `pos_x`, `pos_y`, `is_parameter`, `parameter_name`, `input_names`, `output_names` — the class is `class_name`, **not `expression_class`** |
| `MaterialNodePropertyInfo` (from expression property lists) | `name`, `value`, `property_type`, `is_editable` — it's `name` here, **not `property_name`** |
| `MaterialSummary` (from `MaterialService.summarize`) | `material_name`, `material_path`, `blend_mode`, `shading_model`, `material_domain`, `two_sided`, `expression_count`, `parameter_count`, `parameter_names`, `key_properties`, `editable_properties` — it's `material_name`/`material_path`/`parameter_count`, **not `name`/`path`/`num_parameters`** |
| `MaterialDetailedInfo` (from `MaterialService.get_material_info`) | `material_name`, `material_path`, `blend_mode`, `shading_model`, `material_domain`, `two_sided`, `expression_count`, `texture_sample_count`, `is_material_instance`, `parent_material`, `parameters` — it's `two_sided`, **not `is_two_sided`** |
| `MaterialPropertyInfo_Custom` (from `MaterialService.list_properties` / `get_property_info`) | `property_name`, `display_name`, `property_type`, `current_value`, `allowed_values`, `category`, `is_editable`, `is_advanced` — it's `property_name`/`current_value`, **not `name`/`value`** (differs from `MaterialNodePropertyInfo` above) |

### 🚨 Setting Material Properties: `set_property` Takes Display OR Internal Name

`MaterialService.set_property(path, name, value)` / `get_property` accept **either** the editor
display name (`"Two Sided"`, `"Blend Mode"`, `"Opacity Mask Clip Value"`) **or** the internal
property name (`"TwoSided"`, `"BlendMode"`, `"OpacityMaskClipValue"`) — matching is
case-insensitive and ignores spaces. Both of these work:

```python
unreal.MaterialService.set_property(path, "Two Sided", "true")   # display name
unreal.MaterialService.set_property(path, "BlendMode", "BLEND_Masked")  # internal name
unreal.MaterialService.set_property(path, "OpacityMaskClipValue", "0.33")
unreal.MaterialService.compile_material(path)   # enum/blend changes need a recompile
unreal.MaterialService.save_material(path)
```

> ⚠️ `set_property` returns a **bool** (`True`/`False`), it does **not** raise on an unknown
> property — a `False` means the name didn't resolve or the value didn't parse. Don't assume
> success: check the return, or verify with `get_property` / `summarize` afterward. Enum values
> are the `BLEND_*` / `MSM_*` / `MD_*` identifiers (see below), not the friendly labels.

**Discover legal enum values, don't guess:** `list_properties` / `get_property_info` populate
`allowed_values` for enum properties — including the classic `TEnumAsByte` ones
(`BlendMode`, `ShadingModel`, `MaterialDomain`). Read them instead of probing `unreal` for the
enum class:

```python
for p in unreal.MaterialService.list_properties(path):
    if p.property_name == "BlendMode":
        print(p.allowed_values)
        # ['BLEND_Opaque','BLEND_Masked','BLEND_Translucent','BLEND_Additive', ...]
```

Common material property internal names: `TwoSided`, `BlendMode`, `ShadingModel`,
`MaterialDomain`, `OpacityMaskClipValue`, `bUsedWithStaticLighting`, `DitheredLODTransition`.

---

## Task Index

| Task | Workflow | Sample script |
|------|----------|---------------|
| Create a material (+ params) | `workflows.md` → Create a basic material | `scripts/create_material.pyx` |
| Create a material instance | `workflows.md` → Create a material instance | `scripts/create_instance.pyx` |
| Add texture / math / function / HLSL nodes | `workflows.md` | — |
| Build a graph with batch ops | `workflows.md` → Batch create/connect/set | `scripts/material_graph_batch.pyx` |
| Inspect/export an existing graph | `reference.md` → Export JSON schema | `scripts/export_graph.pyx` |
| Recreate a material from export | `reference.md` → Recreate from export | — |
| Verify wiring / compile | Critical Rules → `get_material_diagnostics` | — |

## Sub-docs

- **`workflows.md`** — create material/instance, texture/math/function/HLSL/collection/switch nodes,
  set properties, and batch create/connect/set.
- **`reference.md`** — parameter types, output names, enum formats, the `export_material_graph` JSON
  schema, recreate-from-export steps, and Material Function authoring.

## Verification

After non-trivial wiring run `MaterialNodeService.get_material_diagnostics(path)` and confirm
`is_compiled_ok` and the expected `referenced_texture_paths` — do **not** rely on `get_used_textures`
(unreliable for multi-branch graphs). `compile_material` + `save_asset` to persist.

## Related skills
- **landscape-materials** — `LandscapeLayerBlend` nodes.
- **landscape-auto-material** — production landscape materials (functions, RVT, instances).
