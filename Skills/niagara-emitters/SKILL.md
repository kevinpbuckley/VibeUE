---
name: niagara-emitters
display_name: Niagara Emitter Colour & Scratch Pad
description: Niagara emitter ColorFromCurve recolouring (hue-shift with NiagaraEmitterService; curve keys read/written with Epic's GetStackInputData / SetStackInputData) and Custom-HLSL scratch-pad module authoring (NiagaraScratchPadService). Everything else about Niagara — systems, emitters, modules, renderers, stack inputs, sim target, compile state, stack issues — is the engine's NiagaraToolsets. Use when the user asks to recolour or hue-shift particles, edit colour curves, or build a scratch-pad/Custom HLSL module.
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
**ColorFromCurve recolouring** and **Custom-HLSL scratch-pad authoring**. For anything else
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
| **ColorFromCurve keyframes** (read, write, add keys) | Epic `GetStackInputData` / `SetStackInputData` on the curve's `ColorCurve` data-interface input — see [colour curves](color.md) |
| **Hue-shift a colour curve** (keep gradient, saturation, alpha) | **VibeUE `NiagaraEmitterService.shift_color_hue`** — see [colour curves](color.md) |
| The object path of a module's script (scratch modules included) | Epic `GetModuleTopology` → `moduleScript`; `create_scratch_module` also returns `script_path` |
| **Custom-HLSL scratch-pad modules** | **VibeUE `NiagaraScratchPadService`** — see [scratch pad](scratch-pad.md) |

**Curve keys are reachable through Epic's stack-input API.** A ColorFromCurve dynamic input holds
its keyframes in a `ColorCurve` data-interface input; address it by walking `inputNameStack` into
the dynamic input:

```python
import json
NS  = "NiagaraToolsets.NiagaraToolset_System"
ref = {"system": {"refPath": "/Game/VFX/NS_Fire.NS_Fire"}, "emitterName": "Flames",
       "scriptName": "ParticleUpdateScript", "moduleName": "Color",
       "inputNameStack": ["Color", "ColorCurve"]}
data = await call_tool(f"{NS}.GetStackInputData", stackInputRef=ref)
pv = json.loads(data["value"]["propertyValues"])     # RedCurve/GreenCurve/BlueCurve/AlphaCurve -> keys
# ... edit pv ...
await call_tool(f"{NS}.SetStackInputData", stackInputRef=ref, inputData={
    "struct": {"refPath": "/Script/NiagaraEditor.NiagaraExt_StackInputData_DataInterface"},
    "value": {"propertyValues": json.dumps(pv)}})
```

**Detect which colour system an emitter uses** with `GetEmitterTopology` / `GetDynamicInputChain`
(on `inputNameStack: ["Color"]`): a ColorFromCurve entry with a `ColorCurve` data-interface input
means curve-driven; otherwise it is value-driven and you set Scale Color / `InitializeParticle`
colour with `SetStackInputData`. A ColorFromCurve curve **overrides** those, so setting them on a
curve-driven emitter has no visible effect. Check first.

**What VibeUE keeps:** `shift_color_hue(system_path, emitter_name, degrees[, module_name])` rotates
the hue of every key in RGB→HSV→RGB, preserving gradient, saturation, intensity, timing and alpha.
Epic has no HSV rotation, so this is the recolour of choice for a curve-driven emitter.

After any change, check `GetSystemCompileState`, then save with
`await call_tool("EditorToolset.AssetToolset.SaveAsset", assetPath=system_path)`.

## Reference pages

- [colour curves](color.md) — ColorFromCurve detection, hue-shift vs. multiply, Epic key editing recipes
- [scratch pad](scratch-pad.md) — Custom-HLSL module authoring, module inputs/outputs, GPU notes


- [scripts/recolor_emitter.txt](scripts/recolor_emitter.txt) - runnable sample
