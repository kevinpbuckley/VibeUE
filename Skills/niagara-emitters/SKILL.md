---
name: niagara-emitters
display_name: Niagara Emitter Colour & Scratch Pad
description: Niagara emitter ColorFromCurve keyframe editing (read/write curve keys, hue-shift) and Custom-HLSL scratch-pad module authoring (NiagaraEmitterService + NiagaraScratchPadService). Everything else about Niagara — systems, emitters, modules, renderers, stack inputs, sim target, compile state, stack issues — is the engine's NiagaraToolsets. Use when the user asks to recolour or hue-shift particles, edit colour curves, or build a scratch-pad/Custom HLSL module.
vibeue_classes:
  - NiagaraEmitterService
  - NiagaraScratchPadService
unreal_classes:
  - NiagaraEmitter
  - NiagaraScript
keywords:
  - niagara emitter
  - niagara color
  - color from curve
  - ColorFromCurve
  - hue shift
  - recolor particles
  - scratch pad
  - custom hlsl
---

# Niagara Emitters — colour curves and the scratch pad

VibeUE keeps only what UE 5.9's `NiagaraToolsets` cannot express. Load this skill for
**ColorFromCurve keyframe editing** and **Custom-HLSL scratch-pad authoring**. For anything else
Niagara, go to the engine toolsets.

## Who owns what

| Task | Use |
|---|---|
| Create a system, add/remove emitters, modules, renderers | Epic **`NiagaraToolsets`** (`CreateNiagaraSystem`, `AddEmitter`, `AddModule`, `AddRenderer`, `RemoveEmitter`, …) via `call_tool` |
| Read/write module and stack input values (what VibeUE used to call rapid-iteration params) | Epic `GetStackInputData` / `SetStackInputData` / `GetStackInputSchema` |
| Solid colour tints, Scale Color values, `InitializeParticle` colour | Epic `SetStackInputData` on that module's colour input |
| Emitter properties, including **sim target** (CPU/GPU) | Epic `SetEmitterData` / `GetEmitterData` |
| User variables, system data, dependencies | Epic `AddUserVariables` / `GetUserVariables` / `GetSystemData` |
| Compile state, stack issues, fixes | Epic `GetSystemCompileState` / `GetStackIssues` / `ApplyStackIssueFix` |
| **ColorFromCurve keyframes** (read, write, hue-shift) | **VibeUE `NiagaraEmitterService`** — see [colour curves](color.md) |
| **Custom-HLSL scratch-pad modules** | **VibeUE `NiagaraScratchPadService`** — see [scratch pad](scratch-pad.md) |

**Why the curve tools stay:** a ColorFromCurve module stores keyframe *data*, not an input value.
Epic's stack-input API sets inputs; it does not reach inside the curve's keys. That is the whole
remaining justification for this service — if you are not editing curve keys, you are in the wrong
place.

## The three tools

```python
import unreal

# Read the keyframes (empty list => this emitter has no ColorFromCurve module)
keys = unreal.NiagaraEmitterService.get_color_curve_keys(system_path, emitter_name)

# Rotate hue while preserving gradient, saturation, intensity and alpha
unreal.NiagaraEmitterService.shift_color_hue(system_path, emitter_name, 120.0)

# Full control: mutate the keys and write them back
unreal.NiagaraEmitterService.set_color_curve_keys(system_path, emitter_name, keys)
```

All three take an optional `module_name`, defaulting to `"ColorFromCurve"`.

**Detecting which colour system an emitter uses** decides where you go next:

```python
keys = unreal.NiagaraEmitterService.get_color_curve_keys(system_path, emitter_name)
if keys:
    pass   # curve-driven: stay here, use shift_color_hue / set_color_curve_keys
else:
    pass   # value-driven: use Epic's SetStackInputData on Scale Color / InitializeParticle
```

A ColorFromCurve module **overrides** Scale Color and `InitializeParticle` colour, so setting those
through Epic's stack-input tools has no visible effect on a curve-driven emitter. Check first.

After any change, compile with Epic's Niagara compile tool, then
`unreal.EditorAssetLibrary.save_asset(system_path)`.

## Reference pages

- [colour curves](color.md) — ColorFromCurve semantics, hue-shift vs. multiply, key editing recipes
- [scratch pad](scratch-pad.md) — Custom-HLSL module authoring, module inputs/outputs, GPU notes


- [scripts/recolor_emitter.txt](scripts/recolor_emitter.txt) - runnable sample
