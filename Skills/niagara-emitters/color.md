---
name: niagara-emitters/color
description: Niagara emitter colour editing — ColorFromCurve detection and semantics, hue-shift vs. multiplicative tint, reading and writing curve keyframes, and where the engine NiagaraToolsets take over.
---

# Niagara emitter colour editing

## ColorFromCurve overrides everything else

Many stock VFX emitters animate colour over particle lifetime with a **ColorFromCurve** module. That
module **overrides** Scale Color and `InitializeParticle` colour — so writing those inputs (through
Epic's `SetStackInputData`) changes nothing you can see.

ColorFromCurve stores **keyframe data**, not an input value. No stack-input write reaches inside it.
That is why VibeUE still ships three curve tools and nothing else for colour.

### Detect first

```python
import unreal

keys = unreal.NiagaraEmitterService.get_color_curve_keys(system_path, emitter_name)
if keys:
    print(f"ColorFromCurve with {len(keys)} keys -> use shift_color_hue / set_color_curve_keys")
else:
    print("value-driven -> use Epic SetStackInputData on Scale Color / InitializeParticle")
```

## Hue-shift, not multiply

A multiplicative tint (Scale Color, or any RGB multiply) **destroys** detail: multiplying by a
channel of `0.0` kills that channel outright, and strong tints flatten the gradient the artist
authored. Rotating hue preserves it.

| Goal | Do this | Keeps artistic detail |
|---|---|---|
| Recolour a curve-driven emitter | `shift_color_hue(...)` | ✅ yes |
| Arbitrary recolour of a curve | read keys → mutate → `set_color_curve_keys(...)` | ✅ yes, you control it |
| Brighten/dim a value-driven emitter | Epic `SetStackInputData` on Scale Color, equal RGB | ✅ yes |
| Tint a value-driven emitter | Epic `SetStackInputData` on Scale Color, non-equal RGB | ⚠️ partial |
| Multiply a curve-driven emitter by zeros | — | ❌ don't |

### shift_color_hue

```python
import unreal

# Degrees around the colour wheel: 120 -> green, 180 -> cyan/opposite, 240 -> blue/purple, -60 -> red
unreal.NiagaraEmitterService.shift_color_hue(system_path, emitter_name, 120.0)
```

It reads every ColorFromCurve keyframe, converts RGB → HSV, rotates the hue, converts back, and
writes the keys again. Gradients, saturation, intensity variation and alpha all survive.

Across several emitters — list the emitter names with Epic's Niagara toolset, then:

```python
for name in emitter_names:
    if unreal.NiagaraEmitterService.get_color_curve_keys(system_path, name):
        unreal.NiagaraEmitterService.shift_color_hue(system_path, name, 120.0)
```

### Reading and writing keys directly

```python
import unreal

keys = unreal.NiagaraEmitterService.get_color_curve_keys(system_path, emitter_name)
for key in keys:
    print(f"t={key.time:.2f}  R={key.r:.2f} G={key.g:.2f} B={key.b:.2f} A={key.a:.2f}")

# Example: invert the colour ramp, leaving alpha and timing untouched
for key in keys:
    key.r = 1.0 - key.r
    key.g = 1.0 - key.g
    key.b = 1.0 - key.b

unreal.NiagaraEmitterService.set_color_curve_keys(system_path, emitter_name, keys)
```

`key.time` is normally 0.0–1.0 across particle lifetime; `r`/`g`/`b` may exceed 1.0 for HDR.
All three tools accept an optional `module_name` (default `"ColorFromCurve"`) when an emitter has a
differently named curve module.

## Then compile and save

```python
# Compile through Epic's Niagara toolset (call_tool), then:
unreal.EditorAssetLibrary.save_asset(system_path)
```

## Everything else is the engine's

Adding or removing a ColorFromCurve module, Scale Color values, spawn rates, sim target, renderers,
emitters, user parameters, compile state and stack issues are all `NiagaraToolsets` via `call_tool`.
Run `describe_toolset` on the Niagara toolsets for exact action names.
