---
name: niagara-emitters/color
description: Niagara emitter color editing — ColorFromCurve detection, choosing the right color-change method (shift_color_hue vs tint vs scale), reading/writing curve keys, per-stage color, and managing ColorFromCurve modules.
---

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

# Apply to multiple emitters (list emitters via the engine NiagaraToolsets — see niagara-systems):
for name in emitter_names:   # names gathered from the engine toolset's list-emitters tool
    keys = unreal.NiagaraEmitterService.get_color_curve_keys(system_path, name)
    if len(keys) > 0:
        unreal.NiagaraEmitterService.shift_color_hue(system_path, name, 120.0)

# Compile via the engine NiagaraToolsets tool, then save:
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

# Compile after changes via the engine NiagaraToolsets tool, then save the asset.
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

> **Module CRUD (remove / add / enable a module) is an engine `NiagaraToolsets` operation** — discover
> the exact tools with `describe_toolset("NiagaraToolsets.NiagaraToolset_System")` and call them via
> `call_tool`. The rapid-iteration-param writes below stay on VibeUE `NiagaraService`. Prefer
> `shift_color_hue` / `set_color_curve_keys` above when you can — they don't require touching modules.

### Strategy 1: Remove ColorFromCurve (For Simple Solid Colors)

```python
# Remove the ColorFromCurve module via the engine NiagaraToolsets remove-module tool, then
# set Scale Color directly (rapid-iteration write stays on VibeUE NiagaraService):
unreal.NiagaraService.set_rapid_iteration_param(
    system_path, emitter_name,
    f"Constants.{emitter_name}.Color.Scale Color",
    "(0.0, 2.0, 0.0)"
)
```

**⚠️ DOWNSIDE:** Removes color animation over lifetime.

### Strategy 2: Add ScaleColor Module After ColorFromCurve

To **keep the curve animation but tint it** — note `set_color_tint` (VibeUE) already does exactly
this (it ensures a ScaleColor module exists and sets its tint), so prefer it:

```python
unreal.NiagaraEmitterService.set_color_tint(system_path, emitter_name, "(0.0, 3.0, 0.0)")
```

If you instead add the ScaleColor module yourself (engine `NiagaraToolsets` add-module tool into the
`ParticleUpdate` stage, after ColorFromCurve), set its scale via the rapid-iteration write:

```python
unreal.NiagaraService.set_rapid_iteration_param(
    system_path, emitter_name,
    f"Constants.{emitter_name}.ScaleColor.Scale RGB",
    "(0.0, 3.0, 0.0)"  # Green tint
)
# Then compile via the engine NiagaraToolsets tool.
```

**✅ BENEFIT:** Preserves the color animation while applying a color tint/shift.

##### Strategy 3: Disable ColorFromCurve (Reversible)

Disable (rather than remove) the ColorFromCurve module with the engine `NiagaraToolsets`
enable-module tool (set enabled=False), then Scale Color works:

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"  # Your Niagara system
emitter_name = "YourEmitter"                  # Target emitter

# After disabling ColorFromCurve via the engine toolset:
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

