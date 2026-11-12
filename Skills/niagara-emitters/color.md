---
name: niagara-emitters/color
description: Niagara emitter colour editing — ColorFromCurve detection and semantics, hue-shift vs. multiplicative tint, reading and writing curve keyframes with Epic's GetStackInputData / SetStackInputData, and VibeUE's shift_color_hue.
---

# Niagara emitter colour editing

## ColorFromCurve overrides everything else

Many stock VFX emitters animate colour over particle lifetime with a **ColorFromCurve** dynamic
input (typically on the `Color` module's `Color` input in `ParticleUpdateScript`). It **overrides**
Scale Color and `InitializeParticle` colour — so writing those inputs changes nothing you can see.

The curve's keyframes live in a `ColorCurve` **data-interface input** inside that dynamic input.
Epic's stack-input API reaches it: address it with an `inputNameStack` that walks into the dynamic
input (`["Color", "ColorCurve"]`) and read/write it with `GetStackInputData` / `SetStackInputData`.
VibeUE keeps exactly one colour tool, `shift_color_hue`, because Epic has no HSV rotation.

All calls below run inside `execute_python` with the injected `await call_tool(...)`.

### Detect first

```python
NS  = "NiagaraToolsets.NiagaraToolset_System"
sys = {"refPath": "/Game/VFX/NS_Fire.NS_Fire"}
color_input = {"system": sys, "emitterName": "Flames", "scriptName": "ParticleUpdateScript",
               "moduleName": "Color", "inputNameStack": ["Color"]}

# Structure of the whole emitter (modules + inputs, no values): find the colour module first.
topo = await call_tool(f"{NS}.GetEmitterTopology",
                       emitterRef={"system": sys, "emitterName": "Flames"})

# The Color input's dynamic-input chain: a ColorFromCurve entry with a ColorCurve
# data-interface input means curve-driven. GetDynamicInputChain errors if the input is not Dynamic
# (i.e. value-driven -> set the colour with SetStackInputData on Scale Color / InitializeParticle).
chain = await call_tool(f"{NS}.GetDynamicInputChain", stackInputRef=color_input)
chain
```

## Hue-shift, not multiply

A multiplicative tint (Scale Color, or any RGB multiply) **destroys** detail: multiplying by a
channel of `0.0` kills that channel outright, and strong tints flatten the gradient the artist
authored. Rotating hue preserves it.

| Goal | Do this | Keeps artistic detail |
|---|---|---|
| Recolour a curve-driven emitter | VibeUE `shift_color_hue(...)` | ✅ yes |
| Arbitrary edit of the curve keys | Epic `GetStackInputData` → mutate → `SetStackInputData` | ✅ yes, you control it |
| Brighten/dim a value-driven emitter | Epic `SetStackInputData` on Scale Color, equal RGB | ✅ yes |
| Tint a value-driven emitter | Epic `SetStackInputData` on Scale Color, non-equal RGB | ⚠️ partial |
| Multiply a curve-driven emitter by zeros | — | ❌ don't |

### shift_color_hue (VibeUE)

```python
import unreal

# Degrees around the colour wheel: 120 -> green, 180 -> cyan/opposite, 240 -> blue/purple, -60 -> red
unreal.NiagaraEmitterService.shift_color_hue(system_path, emitter_name, 120.0)
```

It reads every ColorFromCurve keyframe, converts RGB → HSV, rotates the hue, converts back, and
writes the keys again. Gradients, saturation, intensity variation, timing and alpha all survive.
It returns `False` when the emitter has no colour curve. An optional `module_name` (default
`"ColorFromCurve"`) picks a differently named curve function on the emitter graph.

Across several emitters — list the emitter names with Epic's `GetSystemSummary`, then call
`shift_color_hue` per emitter and keep the ones that returned `True`.

### Reading and writing keys directly (Epic)

```python
import json
NS  = "NiagaraToolsets.NiagaraToolset_System"
ref = {"system": {"refPath": "/Game/VFX/NS_Fire.NS_Fire"}, "emitterName": "Flames",
       "scriptName": "ParticleUpdateScript", "moduleName": "Color",
       "inputNameStack": ["Color", "ColorCurve"]}

data = await call_tool(f"{NS}.GetStackInputData", stackInputRef=ref)
# data is an instanced NiagaraExt_StackInputData_DataInterface; its propertyValues is a JSON STRING
pv = json.loads(data["value"]["propertyValues"])
# pv["RedCurve"/"GreenCurve"/"BlueCurve"/"AlphaCurve"]["keys"] = [{"time", "value", "interpMode", "tangentMode", ...}]

# Example: invert the colour ramp, leaving alpha and timing untouched
for ch in ("RedCurve", "GreenCurve", "BlueCurve"):
    for k in pv[ch]["keys"]:
        k["value"] = 1.0 - k["value"]

await call_tool(f"{NS}.SetStackInputData", stackInputRef=ref, inputData={
    "struct": {"refPath": "/Script/NiagaraEditor.NiagaraExt_StackInputData_DataInterface"},
    "value": {"propertyValues": json.dumps(pv)},
})
```

Adding a key is appending a `{time, value, ...}` entry to a channel's `keys` (copy an existing key
for the tangent fields). `time` is normally 0.0–1.0 across particle lifetime; values may exceed 1.0
for HDR. Each channel is its own curve — keys need not share times across channels.

## Then compile and save

```python
state = await call_tool("NiagaraToolsets.NiagaraToolset_System.GetSystemCompileState",
                        system={"refPath": "/Game/VFX/NS_Fire.NS_Fire"})
# expect no errors (aggregateStatus UpToDate / bHasErrors false)
await call_tool("EditorToolset.AssetToolset.SaveAsset", assetPath="/Game/VFX/NS_Fire")
```

## Everything else is the engine's

Adding or removing a ColorFromCurve dynamic input, Scale Color values, spawn rates, sim target,
renderers, emitters, user parameters, compile state and stack issues are all `NiagaraToolsets`.
Use `search_symbols` / `get_symbol` on the Niagara toolsets for exact tool and parameter names.
