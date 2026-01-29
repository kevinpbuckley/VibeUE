# Niagara Emitters Workflows

Emitter-level operations using NiagaraEmitterService.

---

## ⚠️ CRITICAL: Always Test Compilation After Changes

**After adding/removing modules or changing parameters**, compile and test:

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"  # Replace with your Niagara system path

# After making changes to emitter
# ...

# ALWAYS compile with results to check for errors
result = unreal.NiagaraService.compile_with_results(system_path)

if not result.success:
    print(f"❌ Compilation failed with {result.error_count} errors:")
    for error in result.errors:
        print(f"  - {error}")
    
    # Common fixes:
    # - Missing required modules (Initialize, ParticleState, SolveForcesAndVelocity)
    # - Missing renderer
    # - Invalid parameter values
    # - Module order issues
    
    # FIX THE ERRORS, then recompile:
    result = unreal.NiagaraService.compile_with_results(system_path)
    
if result.success:
    print("✅ Compilation successful")
    unreal.EditorAssetLibrary.save_asset(system_path)
```

**Workflow:** Add module → Compile → Fix errors → Recompile → Verify → Save

---

## 1. Build Emitter from Scratch

Every emitter needs: spawn module, renderer, update modules.

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"  # Replace with your Niagara system path
emitter_name = "MySparks"                     # Name for the new emitter

# Add minimal emitter
unreal.NiagaraService.add_emitter(system_path, "minimal", emitter_name)

# Add renderer (REQUIRED - makes particles visible)
unreal.NiagaraEmitterService.add_renderer(
    system_path, emitter_name,
    "SpriteRenderer",
    "Sprite",
    {}
)

# Add spawn rate module
unreal.NiagaraEmitterService.add_module(
    system_path, emitter_name,
    "/Niagara/Modules/Emitter/SpawnRate.SpawnRate",
    "EmitterUpdate",
    "SpawnRate"
)

# Add initialization module
unreal.NiagaraEmitterService.add_module(
    system_path, emitter_name,
    "/Niagara/Modules/Spawn/Initialization/InitializeParticle.InitializeParticle",
    "ParticleSpawn",
    "Initialize"
)

# Configure spawn rate
unreal.NiagaraService.set_rapid_iteration_param(
    system_path, emitter_name,
    f"Constants.{emitter_name}.SpawnRate.SpawnRate",
    "25"
)

# CRITICAL: Test compilation
result = unreal.NiagaraService.compile_with_results(system_path)
if not result.success:
    print(f"Errors: {result.errors}")
    # Fix errors and recompile
else:
    unreal.EditorAssetLibrary.save_asset(system_path)
unreal.NiagaraService.compile_system(system_path)
```

---

## 2. Configure Rapid Iteration Parameters

Rapid iteration params are module settings that can be adjusted without recompile.

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"  # Replace with your Niagara system path
emitter_name = "YourEmitter"                  # Replace with your emitter name

# List ALL params (shows script stage for each)
params = unreal.NiagaraService.list_rapid_iteration_params(system_path, emitter_name)
for p in params:
    print(f"[{p.script_type}] {p.parameter_name}: {p.value}")

# Set param - sets in ALL stages where it exists
# NOTE: Use full param name from list_rapid_iteration_params (e.g., "Constants.{emitter_name}.Color.Scale Color")
unreal.NiagaraService.set_rapid_iteration_param(
    system_path, emitter_name,
    f"Constants.{emitter_name}.Color.Scale Color",
    "(0.0, 3.0, 0.0)"  # RGB bright green
)

# Set param in SPECIFIC stage
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    system_path, emitter_name,
    "ParticleUpdate",  # Stage name BEFORE param name
    f"Constants.{emitter_name}.Color.Scale Color",
    "(0.0, 1.0, 0.0)"  # Green only in ParticleUpdate
)
```

---

## 3. Manage Modules

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"  # Replace with your Niagara system path
emitter_name = "YourEmitter"                  # Replace with your emitter name

# List available module scripts
scripts = unreal.NiagaraEmitterService.list_available_scripts("Color", "")

# List modules in emitter (by stage)
modules = unreal.NiagaraEmitterService.list_modules(system_path, emitter_name)
for m in modules:
    print(f"[{m.script_type}] {m.module_name}")

# Add module
unreal.NiagaraEmitterService.add_module(
    system_path, emitter_name,
    "/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity",
    "ParticleUpdate",
    "Velocity"
)

# Remove module
unreal.NiagaraEmitterService.remove_module(
    system_path, emitter_name,
    "Velocity",
    "ParticleUpdate"
)
```

---

## 4. Manage Renderers

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"  # Replace with your Niagara system path
emitter_name = "YourEmitter"                  # Replace with your emitter name

# List renderers
renderers = unreal.NiagaraEmitterService.list_renderers(system_path, emitter_name)

# Add sprite renderer
unreal.NiagaraEmitterService.add_renderer(
    system_path, emitter_name,
    "SpriteRenderer",
    "MySprite",
    {"Material": "/Game/Materials/M_Smoke"}
)

# Add mesh renderer
unreal.NiagaraEmitterService.add_renderer(
    system_path, emitter_name,
    "MeshRenderer",
    "MyMesh",
    {"Mesh": "/Game/Meshes/SM_Spark"}
)

# Add ribbon renderer (for trails)
unreal.NiagaraEmitterService.add_renderer(
    system_path, emitter_name,
    "RibbonRenderer",
    "MyRibbon",
    {}
)

# Add light renderer
unreal.NiagaraEmitterService.add_renderer(
    path, emitter,
    "LightRenderer",
    "MyLight",
    {}
)
```

---

## 5. Color Adjustments

### Understanding Niagara Color Systems

Niagara has multiple ways to control particle color. Understanding which system an emitter uses is **critical** before changing colors.

#### Color Control Methods (in order of precedence)

| Method | Where It Lives | How It Works |
|--------|---------------|--------------|
| **ColorFromCurve module** | ParticleUpdate stage | Animates color over particle lifetime using keyframe curves. **OVERRIDES all other color settings.** |
| **Scale Color module** | ParticleSpawn/ParticleUpdate | Multiplies existing color by RGB values. Additive effect. |
| **InitializeParticle.Color** | ParticleSpawn | Sets initial particle color at spawn time. |

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

### ⚠️⚠️ CRITICAL: Choosing the Right Color Change Method

#### The Problem with Color Multiplication (set_color_tint / Scale Color)

`set_color_tint` and Scale Color **MULTIPLY** existing colors by your RGB values:
- **RGB values of 0 ELIMINATE those color channels entirely**
- **RGB values < 1 dim those channels**
- **RGB values > 1 brighten those channels**

This **destroys color gradients** when you use 0 values:

| Original Color | × Tint Value | Result |
|----------------|--------------|--------|
| `(1.0, 0.5, 0.0)` orange | `(0.0, 1.0, 0.0)` | `(0.0, 0.5, 0.0)` - lost red! |
| `(1.0, 0.8, 0.2)` yellow | `(0.0, 1.0, 0.0)` | `(0.0, 0.8, 0.0)` - flat green |
| `(0.8, 0.2, 0.0)` red | `(0.0, 1.0, 0.0)` | `(0.0, 0.2, 0.0)` - very dim |

**Result:** A rich gradient becomes flat because channels with 0 multiplier are eliminated.

#### Method Decision Table

| Goal | Method | Preserves Detail? |
|------|--------|-------------------|
| **Change hue** (recolor effect) | `shift_color_hue()` | ✅ Yes - BEST |
| **Custom curve manipulation** | `get/set_color_curve_keys()` | ✅ Yes |
| **Brighten/dim uniformly** | `set_color_tint()` with equal RGB values | ✅ Yes |
| **Tint toward a color** | `set_color_tint()` with non-zero RGB | ⚠️ Partial |
| **Quick test** (accept detail loss) | `set_color_tint()` with zeros | ❌ No |

---

### ✅ RECOMMENDED: shift_color_hue (For Recoloring Effects)

Use `shift_color_hue` when you want to **change the color** of an effect while preserving all artistic detail (gradients, intensity variations, saturation).

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
    # Check if emitter has color curves
    keys = unreal.NiagaraEmitterService.get_color_curve_keys(system_path, e.emitter_name)
    if len(keys) > 0:
        unreal.NiagaraEmitterService.shift_color_hue(system_path, e.emitter_name, 120.0)

# Always compile and save after changes
unreal.NiagaraService.compile_with_results(system_path)
unreal.EditorAssetLibrary.save_asset(system_path)
```

**How it works internally:**
1. Reads all ColorFromCurve keyframes
2. Converts each RGB value to HSV (Hue, Saturation, Value)
3. Rotates the hue by specified degrees
4. Converts back to RGB
5. Writes modified keyframes back

**Preserves:** gradients, intensity variations, saturation, alpha, all artistic detail.

---

### Reading/Writing Color Curve Keys Directly

For custom color manipulation beyond hue shifting:

```python
import unreal

# Read all keyframes from ColorFromCurve
keys = unreal.NiagaraEmitterService.get_color_curve_keys(system_path, emitter_name)

print(f"Found {len(keys)} keyframes:")
for key in keys:
    print(f"  Time {key.time:.2f}: R={key.r:.2f}, G={key.g:.2f}, B={key.b:.2f}, A={key.a:.2f}")

# Modify keys (examples)
for key in keys:
    # Invert colors
    key.r = 1.0 - key.r
    key.g = 1.0 - key.g
    key.b = 1.0 - key.b
    
    # Or boost saturation
    # Or adjust intensity
    # Or any custom transformation

# Write modified keys back
unreal.NiagaraEmitterService.set_color_curve_keys(system_path, emitter_name, keys)

# Compile after changes
unreal.NiagaraService.compile_with_results(system_path)
```

---

### Using set_color_tint (Quick Tinting)

Use `set_color_tint` for quick color adjustments. **Understand the multiplication behavior!**

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

### Scale Color via Rapid Iteration Parameters

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

#### Setting Different Colors Per Stage (Fade Effects)

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

## 6. Verify Emitter Has Required Components

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"  # Replace with your Niagara system path
emitter_name = "YourEmitter"                  # Replace with your emitter name

# Check renderers
renderers = unreal.NiagaraEmitterService.list_renderers(system_path, emitter_name)
if len(renderers) == 0:
    print("⚠️ No renderer - particles won't be visible!")
    unreal.NiagaraEmitterService.add_renderer(system_path, emitter_name, "SpriteRenderer", "Sprite", {})

# Check modules
modules = unreal.NiagaraEmitterService.list_modules(system_path, emitter_name)

spawn_modules = [m for m in modules if m.script_type == "ParticleSpawn"]
update_modules = [m for m in modules if m.script_type == "ParticleUpdate"]
emitter_update = [m for m in modules if m.script_type == "EmitterUpdate"]

if len(spawn_modules) == 0:
    print("⚠️ No ParticleSpawn modules!")

if len(update_modules) == 0:
    print("⚠️ No ParticleUpdate modules!")

# Check for spawn rate
has_spawn_rate = any("spawn" in m.module_name.lower() for m in emitter_update)
if not has_spawn_rate:
    print("⚠️ No spawn rate module - no particles will spawn!")
```

---

## 7. Graph Positioning

Position emitters in the Niagara system editor graph.

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"  # Replace with your Niagara system path

# Get all emitters
emitters = unreal.NiagaraService.list_emitters(system_path)

# Space emitters evenly (250px apart)
for i, e in enumerate(emitters):
    x = i * 250
    y = 256  # Align Y position
    unreal.NiagaraService.set_emitter_graph_position(system_path, e.emitter_name, x, y)

# Get specific emitter position
pos = unreal.NiagaraService.get_emitter_graph_position(system_path, "YourEmitter")
print(f"Position: ({pos.x}, {pos.y})")
```
