---
name: niagara-emitters
display_name: Niagara Emitters
description: Configure Niagara emitter internals - modules, renderers, rapid iteration parameters, graph positioning, and scratch-pad authoring (Custom HLSL + node graph)
vibeue_classes:
  - NiagaraEmitterService
  - NiagaraScratchPadService
  - NiagaraService
unreal_classes:
  - NiagaraEmitter
  - NiagaraScript
keywords:
  - niagara emitter
  - niagara module
  - add module
  - add renderer
  - rapid iteration
  - particle spawn
  - particle update
  - emitter update
  - spawn rate
  - niagara color
  - color from curve
  - ColorFromCurve
  - NiagaraEmitterService
  - NiagaraScratchPadService
  - scratch pad
  - scratch module
  - custom hlsl
  - parameter map get
  - parameter map set
  - niagara graph
  - splat
  - grid 2d
  - render target
  - add_module
  - add_renderer
  - list_modules
  - set_rapid_iteration_param
  - create_scratch_module
  - add_custom_hlsl_node
  - add_module_input
  - connect_pins
---

## Niagara Emitters Skill

**NiagaraEmitterService** handles emitter-level operations:
- Add/configure modules (spawn, update, render)
- Add/configure renderers (sprite, ribbon, mesh)
- Read/write rapid iteration parameters
- Graph positioning for emitters

**NiagaraScratchPadService** authors scratch-pad module **graphs**:
- Create empty scratch modules on any stage
- Drop Map Get / Map Set / Op / Custom HLSL nodes
- Add typed pins, connect pins, declare module inputs/outputs
- Apply + recompile in one call

For **system-level** operations (create, add emitters, user params), load the `niagara-systems` skill.

---

## ⚠️ CRITICAL: Script Stages

Niagara modules exist in different **script stages**:
- `EmitterSpawn` - Runs once when emitter spawns
- `EmitterUpdate` - Runs every frame for emitter
- `ParticleSpawn` - Runs once when each particle spawns  
- `ParticleUpdate` - Runs every frame for each particle

### ⚠️ IMPORTANT: Parameters Exist in MULTIPLE Stages

**Color.Scale Color**, **Velocity**, **Size**, and other parameters often exist in BOTH ParticleSpawn AND ParticleUpdate stages!

**When changing these parameters, you MUST update BOTH stages:**

**IMPORTANT:** Use full parameter names from `list_rapid_iteration_params` (e.g., `Constants.emitter.Color.Scale Color`)

```python
# ❌ WRONG - Only sets ParticleUpdate, color may not work correctly
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    path, emitter, "ParticleUpdate", f"Constants.{emitter}.Color.Scale Color", "(0.0, 3.0, 0.0)"
)

# ✅ CORRECT - Use set_rapid_iteration_param to set ALL matching stages at once
unreal.NiagaraService.set_rapid_iteration_param(
    path, emitter, f"Constants.{emitter}.Color.Scale Color", "(0.0, 3.0, 0.0)"
)

# ✅ OR explicitly set BOTH stages
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    path, emitter, "ParticleSpawn", f"Constants.{emitter}.Color.Scale Color", "(0.0, 3.0, 0.0)"
)
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    path, emitter, "ParticleUpdate", f"Constants.{emitter}.Color.Scale Color", "(0.0, 3.0, 0.0)"
)
```

**Why?** ParticleSpawn sets initial color, ParticleUpdate multiplies it. If you only set one, the other uses its default value and the result won't be what you expect.

### ⚠️ ColorFromCurve Handling (CRITICAL)

Many VFX emitters use a **ColorFromCurve** module that animates color over particle lifetime. This module **OVERRIDES** any Scale Color or InitializeParticle Color values you set!

**ColorFromCurve uses curve data (keyframes) that CANNOT be modified via rapid iteration parameters.** The curve stores actual keyframe data, not simple scalar values.

#### Detecting Which Color System an Emitter Uses

```python
import unreal

# Check if emitter has ColorFromCurve (curve-based color)
keys = unreal.NiagaraEmitterService.get_color_curve_keys(system_path, emitter_name)
if len(keys) > 0:
    print("Uses ColorFromCurve - must use shift_color_hue or set_color_curve_keys")
else:
    print("Uses Scale Color/InitializeParticle - can use set_rapid_iteration_param")
```

---

## ⚠️⚠️ CRITICAL: Choosing the Right Color Change Method

### The Problem with Color Multiplication (set_color_tint / Scale Color)

`set_color_tint` and Scale Color **MULTIPLY** existing colors by your RGB values:
- **RGB values of 0 ELIMINATE those color channels entirely**
- **RGB values < 1 dim those channels**
- **RGB values > 1 brighten those channels**

This **destroys color gradients** when you use 0 values because multiplication removes that channel.

### When to Use Each Method

| Goal | Method | Preserves Detail? |
|------|--------|-------------------|
| **Change hue** (recolor effect) | `shift_color_hue()` | ✅ Yes - BEST |
| **Custom curve manipulation** | `get/set_color_curve_keys()` | ✅ Yes |
| **Brighten/dim uniformly** | `set_color_tint()` with equal RGB | ✅ Yes |
| **Tint toward a color** | `set_color_tint()` with non-zero RGB | ⚠️ Partial |
| **Quick test** (accept detail loss) | `set_color_tint()` with zeros | ❌ No |

---

## ✅ RECOMMENDED: shift_color_hue (For Recoloring Effects)

Use `shift_color_hue` when you want to **change the color** of an effect while preserving all artistic detail.

```python
import unreal

# Shift hue by degrees (color wheel rotation)
# Common shifts:
#   120° = shift toward green
#   180° = shift toward cyan/opposite
#   240° = shift toward blue/purple
#   -60° = shift toward red

unreal.NiagaraEmitterService.shift_color_hue(system_path, emitter_name, 120.0)

# Apply to multiple emitters
emitters = unreal.NiagaraService.list_emitters(system_path)
for e in emitters:
    keys = unreal.NiagaraEmitterService.get_color_curve_keys(system_path, e.emitter_name)
    if len(keys) > 0:
        unreal.NiagaraEmitterService.shift_color_hue(system_path, e.emitter_name, 120.0)

# Always compile and save after changes
unreal.NiagaraService.compile_with_results(system_path)
unreal.EditorAssetLibrary.save_asset(system_path)
```

### How shift_color_hue Works

1. Reads all ColorFromCurve keyframes
2. Converts each RGB value to HSV (Hue, Saturation, Value)
3. Rotates the hue by specified degrees
4. Converts back to RGB
5. Writes modified keyframes back

**Preserves:** gradients, intensity variations, saturation, alpha, all artistic detail.

---

## Reading/Writing Color Curve Keys Directly

For custom color manipulation beyond hue shifting:

```python
import unreal

# Read all keyframes from ColorFromCurve
keys = unreal.NiagaraEmitterService.get_color_curve_keys(system_path, emitter_name)

print(f"Found {len(keys)} keyframes:")
for key in keys:
    print(f"  Time {key.time:.2f}: R={key.r:.2f}, G={key.g:.2f}, B={key.b:.2f}, A={key.a:.2f}")

# Modify keys (example: invert colors)
for key in keys:
    key.r = 1.0 - key.r
    key.g = 1.0 - key.g  
    key.b = 1.0 - key.b

# Write modified keys back
unreal.NiagaraEmitterService.set_color_curve_keys(system_path, emitter_name, keys)

# Compile after changes
unreal.NiagaraService.compile_with_results(system_path)
```

---

## Using set_color_tint (Quick Tinting)

Use for quick color adjustments. **Understand the multiplication behavior!**

```python
import unreal

# ✅ GOOD: Brighten all channels equally (preserves gradients)
unreal.NiagaraEmitterService.set_color_tint(system_path, emitter_name, "(2.0, 2.0, 2.0)")

# ✅ GOOD: Slight tint toward green (preserves most detail)
unreal.NiagaraEmitterService.set_color_tint(system_path, emitter_name, "(0.5, 1.5, 0.5)")

# ⚠️ CAUTION: Strong tint (loses some red/blue detail)
unreal.NiagaraEmitterService.set_color_tint(system_path, emitter_name, "(0.2, 2.0, 0.2)")

# ❌ DESTRUCTIVE: Zero values eliminate channels entirely
unreal.NiagaraEmitterService.set_color_tint(system_path, emitter_name, "(0.0, 2.0, 0.0)")

# With custom alpha multiplier
unreal.NiagaraEmitterService.set_color_tint(system_path, emitter_name, "(1.5, 1.5, 1.5)", 0.8)
```

---

## Scale Color via Rapid Iteration Parameters

For emitters **without** ColorFromCurve, use Scale Color directly:

```python
import unreal

# ⚠️ Scale Color often exists in BOTH stages - they multiply together!
# Use set_rapid_iteration_param to set ALL matching stages at once

# First, list params to get exact names
params = unreal.NiagaraService.list_rapid_iteration_params(system_path, emitter_name)
for p in params:
    if "Color" in p.parameter_name:
        print(f"[{p.script_type}] {p.parameter_name}: {p.value}")

# Set Scale Color (updates ALL stages where it exists)
unreal.NiagaraService.set_rapid_iteration_param(
    system_path, emitter_name,
    f"Constants.{emitter_name}.Color.Scale Color",
    "(0.0, 2.0, 0.0)"  # RGB format
)

# Set InitializeParticle Color (RGBA format)
unreal.NiagaraService.set_rapid_iteration_param(
    system_path, emitter_name,
    f"Constants.{emitter_name}.InitializeParticle001.Color",
    "(0.0, 1.0, 0.0, 1.0)"
)
```

### Setting Different Colors Per Stage (Fade Effects)

```python
# Particles start one color...
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    system_path, emitter_name,
    "ParticleSpawn",
    f"Constants.{emitter_name}.Color.Scale Color",
    "(0.0, 1.0, 0.0)"  # Green at spawn
)

# ...and transition to another over lifetime
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    system_path, emitter_name,
    "ParticleUpdate", 
    f"Constants.{emitter_name}.Color.Scale Color",
    "(1.0, 0.0, 0.0)"  # Red over time
)
```

---

## Managing ColorFromCurve Modules

### Strategy 1: Remove ColorFromCurve (For Simple Solid Colors)

```python
# Remove curve to allow Scale Color to work
unreal.NiagaraEmitterService.remove_module(system_path, emitter_name, "ColorFromCurve")

# Now set Scale Color directly
unreal.NiagaraService.set_rapid_iteration_param(
    system_path, emitter_name,
    f"Constants.{emitter_name}.Color.Scale Color",
    "(0.0, 2.0, 0.0)"
)
```

**⚠️ DOWNSIDE:** Removes color animation over lifetime.

### Strategy 2: Add ScaleColor Module After ColorFromCurve

To **keep the curve animation but tint it**:

```python
# Add ScaleColor module to ParticleUpdate (after ColorFromCurve)
unreal.NiagaraEmitterService.add_module(
    system_path, emitter_name,
    "/Niagara/Modules/Update/Color/ScaleColor.ScaleColor",
    "Update"
)

# Set the scale RGB via rapid iteration params
unreal.NiagaraService.set_rapid_iteration_param(
    path, emitter,
    f"Constants.{emitter}.ScaleColor.Scale RGB",
    "(0.0, 3.0, 0.0)"  # Green tint
)

# Compile
unreal.NiagaraService.compile_with_results(path)
```

**✅ BENEFIT:** Preserves the color animation while applying a color tint/shift.

##### Strategy 3: Disable ColorFromCurve (Reversible)

If you want to temporarily disable the curve without removing it:

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"  # Your Niagara system
emitter_name = "YourEmitter"                  # Target emitter

# Disable instead of remove (can re-enable later)
unreal.NiagaraEmitterService.enable_module(system_path, emitter_name, "ColorFromCurve", False)

# Now Scale Color will work
unreal.NiagaraService.set_rapid_iteration_param(
    system_path, emitter_name, f"Constants.{emitter_name}.Color.Scale Color", "(0.0, 3.0, 0.0)"
)
```

##### Color Module Priority (Execution Order)

Modules in ParticleUpdate execute in order. Later modules can override earlier ones:

1. `InitializeParticle` - Sets initial color (ParticleSpawn)
2. `ColorFromCurve` - Animates color over lifetime (ParticleUpdate)
3. `ScaleColor` - Multiplies current color (ParticleUpdate)

**To tint ColorFromCurve output:** Add ScaleColor AFTER it in the stack.
**To replace ColorFromCurve:** Remove it or disable it.

---

## Quick Reference

```python
import unreal

# Generic variable pattern - replace with your actual paths
system_path = "/Game/Path/To/NS_YourSystem"
emitter_name = "YourEmitter"

# === MODULES ===
# List available module scripts
scripts = unreal.NiagaraEmitterService.list_available_scripts("Sprite", "")

# Add module to emitter
unreal.NiagaraEmitterService.add_module(
    system_path, emitter_name,
    "/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity",
    "ParticleUpdate",
    "Velocity"
)

# List modules in emitter
modules = unreal.NiagaraEmitterService.list_modules(system_path, emitter_name)
for m in modules:
    print(f"{m.script_type}: {m.module_name}")

# === RENDERERS ===
# Add sprite renderer
unreal.NiagaraEmitterService.add_renderer(
    system_path, emitter_name,
    "SpriteRenderer",
    "MySprite",
    {"Material": "/Game/Materials/M_Smoke"}
)

# Renderer types: SpriteRenderer, RibbonRenderer, MeshRenderer, LightRenderer

# === RAPID ITERATION PARAMETERS ===
# List all params (shows which stage they're in)
params = unreal.NiagaraService.list_rapid_iteration_params(system_path, emitter_name)
for p in params:
    print(f"[{p.script_type}] {p.parameter_name}: {p.value}")

# OR use NiagaraEmitterService.get_rapid_iteration_parameters for more detail
params = unreal.NiagaraEmitterService.get_rapid_iteration_parameters(system_path, emitter_name)
for p in params:
    print(f"[{p.script_type}] {p.input_name}: {p.current_value}")

# Set param in ALL matching stages (recommended for color)
# Use full param name from list output (e.g., "Constants.YourEmitter.Color.Scale Color")
unreal.NiagaraService.set_rapid_iteration_param(
    system_path, emitter_name,
    f"Constants.{emitter_name}.Color.Scale Color",
    "(0.0, 3.0, 0.0)"  # RGB - bright green
)

# Set param in SPECIFIC stage (for precise control)
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    system_path, emitter_name,
    "ParticleUpdate",  # Stage name comes BEFORE param name
    f"Constants.{emitter_name}.Color.Scale Color",
    "(0.0, 3.0, 0.0)"
)

# === GRAPH POSITIONING ===
# Move emitter in Niagara editor graph
pos = unreal.NiagaraService.get_emitter_graph_position(system_path, emitter_name)
unreal.NiagaraService.set_emitter_graph_position(system_path, emitter_name, pos.x + 250, pos.y)
```

---

## Common Property Names

**NOTE:** Use full parameter names from `list_rapid_iteration_params`. The format is: `Constants.<emitter>.<module>.<property>`

| Property | Type | Stage | Example Value |
|----------|------|-------|---------------|
| `Constants.<e>.SpawnRate.SpawnRate` | Float | EmitterUpdate | `"50"` |
| `Constants.<e>.SpawnBurst.SpawnCount` | Int | EmitterSpawn | `"100"` |
| `Constants.<e>.Color.Scale Color` | Vector3 | ParticleSpawn, ParticleUpdate | `"(0.0, 1.0, 0.0)"` |
| `Constants.<e>.InitializeParticle001.Color` | Color | ParticleSpawn | `"(0.0, 1.0, 0.0, 1.0)"` |
| `Constants.<e>.Lifetime.Lifetime` | Float | ParticleSpawn | `"2.0"` |
| `Constants.<e>.Velocity.Velocity` | Vector | ParticleSpawn | `"(0.0, 0.0, 100.0)"` |

**Value Formats:**
- Float: `"50.0"`
- Vector3 (RGB): `"(0.0, 3.0, 0.0)"` 
- Color (RGBA): `"(0.0, 3.0, 0.0, 1.0)"`
- Vector2: `"(64.0, 64.0)"`
- Vector3: `"(0.0, 0.0, 100.0)"` |

---

## Building Emitters from Scratch

Minimal emitter needs:
1. **Spawn module** (one of):
   - Spawn Rate for continuous
   - Spawn Burst Instantaneous for one-shot
2. **Renderer** (SpriteRenderer minimum)
3. **Particle Update** modules for behavior

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"  # Your Niagara system

# Create empty emitter
unreal.NiagaraService.add_emitter(system_path, "minimal", "MySparks")

# Add spawn
unreal.NiagaraEmitterService.add_module(
    system_path, "MySparks",
    "/Niagara/Modules/Emitter/SpawnRate.SpawnRate",
    "EmitterUpdate", "SpawnRate"
)

# Add renderer
unreal.NiagaraEmitterService.add_renderer(
    system_path, "MySparks",
    "SpriteRenderer", "Sprite", {}
)

# Configure spawn rate
unreal.NiagaraEmitterService.set_rapid_iteration_param(
    system_path, "MySparks",
    "SpawnRate", "25"
)

# Compile to apply
unreal.NiagaraService.compile_system(system_path)
```

---

## Scratch-Pad Authoring (NiagaraScratchPadService)

A **scratch-pad module** is a user-authored UNiagaraScript stored inside an emitter (or system) rather than being a shared asset. It is where you build custom per-particle logic — Map Get reads + math + Custom HLSL + Map Set writes — for one specific effect. `NiagaraEmitterService` can only see scratch modules as opaque "Custom" function-call nodes; **`NiagaraScratchPadService` is the API that reaches inside them and builds the graph**.

### When to use the Scratch Pad service

| Task | Use |
|------|-----|
| Add a built-in module from `/Niagara/Modules/...` | `NiagaraEmitterService.add_module` |
| Set a value on an existing module input | `NiagaraEmitterService.set_module_input` |
| Build per-particle math / splat logic in HLSL | **`NiagaraScratchPadService`** |
| Wire a Custom HLSL node with typed inputs/outputs | **`NiagaraScratchPadService`** |
| Add Map Get / Map Set / Op / If nodes | **`NiagaraScratchPadService`** |

### The pipeline (mental model)

```
Emitter Stage Stack:  [Spawn]→[Update]→...→[YOUR SCRATCH MODULE]→[Output]
                                              │
Scratch graph inside that module:
   [Map In] → [Map Get  Module.X] → [Custom HLSL / Op nodes] → [Map Set  Output.Y] → [Map Out]
```

The default module template (used by `create_scratch_module`) already wires Map In → Map Get → Map Set → Map Out for you. You add nodes between Map Get and Map Set and connect them.

### End-to-end: build a Custom-HLSL splat module

```python
import unreal

S   = "/Game/VFX/NS_TrackPainter"
E   = "CompletelyEmpty"

# 1. Create the empty scratch module on the Particle Update stage
r = unreal.NiagaraScratchPadService.create_scratch_module(S, E, "ParticleUpdate", "SplatLine")
assert r.b_success, r.message
MOD = r.module_name           # e.g. "SplatLine"
print("script path:", r.script_path)

# 2. Declare the module inputs (becomes "Module.X" pins on the Map Get node, and
#    exposes 'X' as a stack input in the emitter)
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "StartPositions", "ArrayVector")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "EndPositions",   "ArrayVector")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "TireRadius",     "float")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "VolumeMin",      "Vector")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "VolumeMax",      "Vector")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "DecayRate",      "float")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "TracksGrid",     "Grid2D")

# 3. Drop a Custom HLSL node with the splat math
hlsl_code = """
// Inputs (variable names match pin names)
int Idx = ExecutionIndex;
Float3 A = StartPositions.Get(Idx);
Float3 B = EndPositions.Get(Idx);
Float3 Dir = normalize(B - A);
// ... your line-rasterization splat code that writes into TracksGrid ...
Splatted = 1;
"""
node = unreal.NiagaraScratchPadService.add_custom_hlsl_node(S, E, MOD, hlsl_code, 200, 0)
HLSL = node.node_id

# 4. Declare typed pins on the Custom HLSL node so it can take inputs and write outputs
for n,t in [("StartPositions","ArrayVector"),("EndPositions","ArrayVector"),
            ("TireRadius","float"),("VolumeMin","Vector"),("VolumeMax","Vector"),
            ("DecayRate","float"),("TracksGrid","Grid2D")]:
    unreal.NiagaraScratchPadService.add_pin(S, E, MOD, HLSL, "Input",  t, n)
unreal.NiagaraScratchPadService.add_pin(S, E, MOD, HLSL, "Output", "bool", "Splatted")

# 5. Wire Map Get → Custom HLSL → Map Set
# (find the Map Get / Map Set IDs from the freshly populated graph)
nodes = unreal.NiagaraScratchPadService.list_nodes(S, E, MOD)
mapget = next(n for n in nodes if n.node_type == "MapGet").node_id
mapset = next(n for n in nodes if n.node_type == "MapSet").node_id

for v in ["StartPositions","EndPositions","TireRadius","VolumeMin","VolumeMax","DecayRate","TracksGrid"]:
    unreal.NiagaraScratchPadService.connect_pins(S, E, MOD, mapget, f"Module.{v}", HLSL, v)

unreal.NiagaraScratchPadService.add_module_output(S, E, MOD, "Particles.Splatted", "bool")
unreal.NiagaraScratchPadService.connect_pins(S, E, MOD, HLSL, "Splatted", mapset, "Particles.Splatted")

# 6. Apply + recompile + save (one call)
unreal.NiagaraScratchPadService.apply_changes(S)
```

### Supported pin types (TypeName strings)

| String | Niagara type |
|--------|--------------|
| `float`, `int`, `bool` | Scalars |
| `Vector`/`Vec3`, `Vector2`/`Vec2`, `Vector4`/`Vec4` | Vectors |
| `Position`, `Color`/`LinearColor` | Special |
| `ArrayFloat`, `ArrayVector`/`ArrayFloat3`, `ArrayFloat2`, `ArrayFloat4`, `ArrayInt`, `ArrayPosition` | DI arrays |
| `Grid2D`/`Grid2DCollection` | Grid 2D Collection DI |
| `RenderTarget2D`/`DynamicRT` | Render Target 2D DI |

### Op node names (`add_op_node`)

Niagara ops live in the `Numeric::` namespace and the service auto-prefixes bare names:

| Pass | Resolves to |
|------|-------------|
| `Add`, `Multiply`, `Subtract`, `Divide` | `Numeric::Add` … |
| `Dot`, `Cross`, `Normalize`, `Length` | `Numeric::Dot` … |
| `Lerp`, `Clamp`, `Min`, `Max`, `Saturate` | `Numeric::Lerp` … |
| `Sin`, `Cos`, `Tan`, `ASin`, `ACos` | `Numeric::Sin` … |

If a bare name doesn't resolve, pass the full `Namespace::Op` string.

### Niagara Custom HLSL gotchas (NOT plain HLSL)

- Reference pin variables by their **pin name** verbatim (`StartPositions`, not `_in_StartPositions`).
- Use **`Float3`**, **`Float2`**, **`Float4`** (capitalized) for vector types in the HLSL body, not `float3`.
- Array DI calls use the generated function names, e.g. `StartPositions.Get(Index)` returns a `Float3`.
- Grid 2D writes use `TracksGrid.SetVector4Value(AttributeIndex, X, Y, Value)` style — see the Niagara docs for the exact signatures the translator generates.
- For pure node-based logic without HLSL, use `add_op_node` + `connect_pins` between Map Get / Map Set.

### One pattern that fixes most "input not found" errors

When the prior session called `NiagaraEmitterService.set_module_input` with a bare name (`"GridWidth"`) on a `SetVariables` module, it returned NOT FOUND because the pin is actually named `"Module.GridWidth"`. **The service now tries `Module.`, `Output.`, and `Particles.` prefixes automatically** and prints the list of available pin names if none match — so re-run and read the log message.

### What `apply_changes` does

A single call:
1. Walks every stack module across every emitter and calls `RefreshFromExternalChanges()` on each one referencing a scratch script (so new module inputs/outputs propagate to the stack UI).
2. Calls `FNiagaraStackGraphUtilities::RebuildEmitterNodes` to rebuild the system's compiled emitter nodes.
3. Marks dirty, recompiles, waits, saves.

Call it **once** at the end of a batch of graph edits — not after every node creation.

### Discovery

```python
import unreal
methods = unreal.SystemLibrary.get_function_names(unreal.NiagaraScratchPadService.static_class())
# or via VibeUE:
print(unreal.discover_python_class("unreal.NiagaraScratchPadService"))
```
