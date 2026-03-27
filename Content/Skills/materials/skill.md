---
name: materials
display_name: Material System
description: Create and edit materials and material instances using MaterialService and MaterialNodeService
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

# Material System Skill

## Critical Rules

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

Use `discover_python_class()` first:
- `MaterialExpressionTypeInfo` uses `display_name`, NOT `name`
- `MaterialOutputConnectionInfo` uses `connected_expression_id`, NOT `expression_id`

---

## Workflows

### Create Basic Material

```python
import unreal

# create_material returns MaterialCreateResult — extract .asset_path
result = unreal.MaterialService.create_material("M_Character", "/Game/Materials/")
if not result.success:
    print(f"FAILED: {result.error_message}")
path = result.asset_path

# create_parameter returns MaterialExpressionInfo — extract .id
# default_value formats: "R,G,B" or "R,G,B,A" or "(R=1.0,G=0.0,B=0.0,A=1.0)"
color_expr = unreal.MaterialNodeService.create_parameter(path, "Vector", "BaseColor", "Surface", "0.8,0.2,0.2,1.0", -500, 0)
unreal.MaterialNodeService.connect_to_output(path, color_expr.id, "", "BaseColor")

# Add roughness
rough_expr = unreal.MaterialNodeService.create_parameter(path, "Scalar", "Roughness", "Surface", "0.5", -500, 100)
unreal.MaterialNodeService.connect_to_output(path, rough_expr.id, "", "Roughness")

unreal.MaterialService.compile_material(path)
unreal.EditorAssetLibrary.save_asset(path)

# Auto-layout: arrange nodes in clean left-to-right columns
unreal.MaterialNodeService.layout_expressions(path)
```

### Create Material Instance

```python
import unreal

# create_instance returns MaterialCreateResult — extract .asset_path
result = unreal.MaterialService.create_instance("/Game/Materials/M_Character", "MI_PlayerRed", "/Game/Materials/")
if not result.success:
    print(f"FAILED: {result.error_message}")
instance_path = result.asset_path

unreal.MaterialService.set_instance_vector_parameter(instance_path, "BaseColor", 1.0, 0.0, 0.0, 1.0)
unreal.MaterialService.set_instance_scalar_parameter(instance_path, "Roughness", 0.3)
unreal.EditorAssetLibrary.save_asset(instance_path)
```

### Add Texture Parameter

```python
import unreal

tex_expr = unreal.MaterialNodeService.create_parameter(path, "Texture", "DiffuseMap", "Textures", "", -500, 0)
unreal.MaterialNodeService.connect_to_output(path, tex_expr.id, "", "BaseColor")
unreal.MaterialService.compile_material(path)
```

### Create Math Expression

```python
import unreal

path = "/Game/Materials/M_Tint"

# All create_* calls return expression info objects — use .id
color_expr = unreal.MaterialNodeService.create_parameter(path, "Vector", "TintColor", "Surface", "", -600, 0)
mult_expr = unreal.MaterialNodeService.create_expression(path, "Multiply", -300, 0)
intensity_expr = unreal.MaterialNodeService.create_parameter(path, "Scalar", "Intensity", "Surface", "1.0", -600, 100)

unreal.MaterialNodeService.connect_expressions(path, color_expr.id, "", mult_expr.id, "A")
unreal.MaterialNodeService.connect_expressions(path, intensity_expr.id, "", mult_expr.id, "B")
unreal.MaterialNodeService.connect_to_output(path, mult_expr.id, "", "BaseColor")
unreal.MaterialService.compile_material(path)
```

### Set Material Properties

```python
import unreal

unreal.MaterialService.set_blend_mode(path, "Translucent")  # For transparency
unreal.MaterialService.set_shading_model(path, "DefaultLit")
unreal.MaterialService.set_two_sided(path, True)
unreal.MaterialService.compile_material(path)
```

### Get Material Info

```python
import unreal

info = unreal.MaterialService.get_material_info("/Game/Materials/M_Character")
if info:
    print(f"Material: {info.name}, Blend Mode: {info.blend_mode}")
```

### Create Function Call Node

Use for MaterialFunction references (e.g., `/Engine/Functions/...`):

```python
import unreal

path = "/Game/Materials/M_Complex"

# create_function_call returns MaterialExpressionInfo — use .id
func_expr = unreal.MaterialNodeService.create_function_call(
    path,
    "/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals",
    -600, 0
)

# Connect like any other node
unreal.MaterialNodeService.connect_to_output(path, func_expr.id, "", "Normal")
unreal.MaterialService.compile_material(path)
```

### Create Custom HLSL Expression

```python
import unreal

path = "/Game/Materials/M_Custom"

# create_custom_expression returns MaterialExpressionInfo — use .id
custom_expr = unreal.MaterialNodeService.create_custom_expression(
    path,
    "return sin(Time * Speed);",     # HLSL code
    "SineWave",                       # Description
    "Float1",                         # OutputType: Float1, Float2, Float3, Float4, MaterialAttributes
    "Time,Speed",                     # Comma-separated input names
    -500, 0
)

# Connect inputs and outputs normally
unreal.MaterialNodeService.connect_to_output(path, custom_expr.id, "", "EmissiveColor")
unreal.MaterialService.compile_material(path)
```

### Create Collection Parameter

Reference a parameter from a MaterialParameterCollection:

```python
import unreal

path = "/Game/Materials/M_Global"

# create_collection_parameter returns MaterialExpressionInfo — use .id
coll_expr = unreal.MaterialNodeService.create_collection_parameter(
    path,
    "/Game/Materials/MPC_GlobalParams",   # collection asset path
    "WindStrength",                        # parameter name in collection
    -500, 0
)

unreal.MaterialNodeService.connect_expressions(path, coll_expr.id, "", some_mult_id, "B")
unreal.MaterialService.compile_material(path)
```

### Add Static Switch Parameter

```python
import unreal

path = "/Game/Materials/M_Switchable"

# Create a static switch (compile-time boolean branch)
switch_id = unreal.MaterialNodeService.create_parameter(
    path, "StaticSwitch", "UseDetailTexture", "Features", "true", -600, 0
)

# StaticSwitch has True/False/Value inputs — connect other nodes to those
```

### Add Texture Object Parameter

```python
import unreal

path = "/Game/Materials/M_Objects"

# TextureObject exposes texture without a sampler (for custom sampling)
tex_obj_id = unreal.MaterialNodeService.create_parameter(
    path, "TextureObject", "DetailMap", "Textures",
    "/Game/Textures/T_Detail.T_Detail", -500, 0
)
```

### Batch Create Expressions

Create many nodes in a single transaction (much faster than individual calls):

```python
import unreal

path = "/Game/Materials/M_Complex"

# Arrays must be same length — one entry per node
types = ["Multiply", "Add", "Lerp", "OneMinus"]
names = ["Mult1", "Add1", "Lerp1", "Invert1"]  # optional display names (use "" to skip)
x_positions = [-400, -400, -200, -600]
y_positions = [0, 200, 100, 0]

result = unreal.MaterialNodeService.batch_create_expressions(
    path, types, names, x_positions, y_positions
)
# result contains all created node IDs
```

### Batch Connect Expressions

Wire up many connections in a single transaction:

```python
import unreal

path = "/Game/Materials/M_Complex"

# Each array entry defines one connection
source_ids = [color_id, rough_id, mult_id]
output_names = ["", "", ""]           # "" = first output
target_ids = [mult_id, mult_id, ""]   # "" = material output
input_names = ["A", "B", "BaseColor"]

result = unreal.MaterialNodeService.batch_connect_expressions(
    path, source_ids, output_names, target_ids, input_names
)
```

### Batch Set Properties

Set many properties across multiple nodes in one call:

```python
import unreal

path = "/Game/Materials/M_Complex"

node_ids = [tex_id, tex_id, const_id]
property_names = ["Texture", "SamplerType", "R"]
property_values = ["/Game/Textures/T_Diffuse", "SAMPLERTYPE_Color", "0.5"]

result = unreal.MaterialNodeService.batch_set_properties(
    path, node_ids, property_names, property_values
)
```

### Export Material Graph (JSON)

Export the entire material graph for analysis or recreation:

```python
import unreal

path = "/Game/Materials/M_Landscape"

# Get full JSON representation of the material graph
json_str = unreal.MaterialNodeService.export_material_graph(path)

# Parse and inspect
import json
graph = json.loads(json_str)

print(f"Expressions: {len(graph['expressions'])}")
print(f"Connections: {len(graph['connections'])}")

# Each expression includes: id, class, class_full, pos_x, pos_y,
# properties (dict), inputs (list), outputs (list)
# Parameter expressions also have: is_parameter, parameter_name, group
# Function calls have: function_path
# Custom expressions have: hlsl_code, output_type, custom_input_names
for expr in graph['expressions']:
    print(f"  {expr['class']} at ({expr['pos_x']}, {expr['pos_y']})")
    if expr.get('is_parameter'):
        print(f"    Parameter: {expr['parameter_name']} (group: {expr['group']})")
```

### ⚠️ Export JSON Schema Reference

**Top-level keys:**
- `material` — `{blend_mode, shading_model, two_sided}`
- `expressions` — array of expression objects
- `connections` — array of connection objects
- `output_connections` — array of material output connection objects

**Expression object keys:**
- `id` — unique expression identifier (use for connections)
- `class` — short class name (e.g. `"Add"`, `"Multiply"`, `"ScalarParameter"`)
- `class_full` — full UE class name (e.g. `"MaterialExpressionAdd"`)
- `pos_x`, `pos_y` — editor position (NOT `x`/`y`)
- `properties` — dict of editable property name→value (excludes ParameterName, Group)
- `inputs` — list of input pin names
- `outputs` — list of output pin names
- `is_parameter` — true for parameter expressions
- `parameter_name` — the parameter's display name (only on parameters)
- `group` — parameter group name (only on parameters)
- `function_path` — material function asset path (only on function calls)
- `hlsl_code` — HLSL code string (only on Custom expressions)
- `custom_input_names` — comma-separated input names (only on Custom expressions)
- `landscape_layers` — array of layer configs (only on LandscapeLayerBlend)

**Connection object keys:**
- `source_id` — expression ID of the source node
- `source_output_index` — integer index of the source output pin
- `source_output_name` — name of the source output pin (empty string for default pin)
- `target_id` — expression ID of the target node
- `target_input` — name of the target input pin (NOT `target_input_name`)

**Output connection object keys (material property connections):**
- `property` — material property name (e.g. `"BaseColor"`, `"Normal"`)
- `expression_id` — expression ID connected to this output
- `output_index` — which output pin index of the expression
- `output_name` — the output pin name (empty string for default)

### ⚠️ Enum Value Format

When setting material properties via `set_property`, enum values accept:
- **Full prefixed names**: `"BLEND_Masked"`, `"MSM_DefaultLit"`, `"MD_Surface"`
- **Short suffix names**: `"Masked"`, `"DefaultLit"`, `"Surface"`
- Both are accepted — the API uses fuzzy matching

### Recreate Material from Export

Use `export_material_graph` + batch operations for material recreation:

```python
import unreal, json

# 1. Export source material
source_json = unreal.MaterialNodeService.export_material_graph("/Game/Materials/M_Source")
graph = json.loads(source_json)

# 2. Create new material — extract .asset_path from result
result = unreal.MaterialService.create_material("M_Source_Copy", "/Game/Materials/")
new_path = result.asset_path

# 3. Set material properties
mat = graph['material']
unreal.MaterialService.set_blend_mode(new_path, mat['blend_mode'])
unreal.MaterialService.set_shading_model(new_path, mat['shading_model'])

# 4. Batch create all expressions
# Use class_full or class (both accepted by create APIs)
types = [e['class'] for e in graph['expressions']]
x_positions = [e['pos_x'] for e in graph['expressions']]
y_positions = [e['pos_y'] for e in graph['expressions']]

# For function calls and specialized types, use batch_create_specialized
# For generic types, use batch_create_expressions

# 5. Set parameter names FIRST (before batch_set_properties)
# The export's parameter_name field has the correct name
for expr in graph['expressions']:
    if expr.get('is_parameter'):
        unreal.MaterialNodeService.set_expression_property(
            new_path, new_id_map[expr['id']], "ParameterName", expr['parameter_name'])
        if expr.get('group'):
            unreal.MaterialNodeService.set_expression_property(
                new_path, new_id_map[expr['id']], "Group", expr['group'])

# 6. Batch set all other properties (ParameterName/Group excluded from export)

# 7. Batch connect using connections array
source_ids = [new_id_map[c['source_id']] for c in graph['connections']]
output_names = [c['source_output_name'] for c in graph['connections']]
target_ids = [new_id_map[c['target_id']] for c in graph['connections']]
input_names = [c['target_input'] for c in graph['connections']]

unreal.MaterialNodeService.batch_connect_expressions(
    new_path, source_ids, output_names, target_ids, input_names)

# 8. Connect output_connections (material property outputs)
for oc in graph['output_connections']:
    unreal.MaterialNodeService.connect_to_output(
        new_path, new_id_map[oc['expression_id']],
        oc['output_name'], oc['property'])

# 9. Compile
unreal.MaterialService.compile_material(new_path)
```

---

## Material Functions

For creating and editing **Material Functions** (reusable node subgraphs):

- Use `MaterialNodeService.get_function_info(function_path)` to inspect a function's inputs/outputs
- Use `MaterialNodeService.export_function_graph(function_path)` to export a function's complete node graph as JSON
- Use `MaterialNodeService.create_material_function(name, dir)` to create new functions
- Use `MaterialNodeService.add_function_input/output(...)` to add I/O pins
- Use `MaterialNodeService.create_function_call(mat_path, func_path)` to reference functions in material graphs
- Load the `landscape-auto-material` skill for landscape-specific material function patterns

## Related Skills

- **landscape-materials**: Landscape materials with `LandscapeLayerBlend` nodes
- **landscape-auto-material**: Production landscape materials using material functions, RVT, and instances
