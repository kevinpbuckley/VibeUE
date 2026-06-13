---
name: niagara-emitters
display_name: Niagara Emitters
description: Configure Niagara emitter internals — modules, renderers, rapid-iteration parameters, color/curve editing, graph positioning, and Custom-HLSL scratch-pad authoring. Use when the user asks to add modules/renderers to a Niagara emitter, recolor or hue-shift particles, set emitter parameters, build an emitter from scratch, or author a scratch-pad/Custom HLSL module. For system-level lifecycle, load niagara-systems.
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

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "niagara-vfx"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

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

### ⚠️ `add_module` stage string must be EXACT — and matters which stage

`add_module` takes exactly **4 args** — `add_module(system_path, emitter_name, module_script_path, stage)`. There is **no 5th "name" argument** (passing one raises `TypeError: takes at most 4 arguments`).

The `stage` must be **one of the four strings above, spelled exactly** (case-insensitive): `ParticleSpawn`, `ParticleUpdate`, `EmitterSpawn`, `EmitterUpdate`. Anything else — `"Spawn"`, `"Update"`, `"Emitter Update"`, `""` — is rejected and `add_module` returns **`False`** without adding anything. (Older builds silently dumped the module into `ParticleUpdate` instead; check the return value.)

**Put each module in its correct stage** — this is the #1 cause of a system that "compiles" but is invalid:

| Module | Correct stage |
|--------|---------------|
| `InitializeParticle` | `ParticleSpawn` |
| `SpawnRate`, `SpawnBurstInstantaneous`, `EmitterState` | `EmitterUpdate` |
| `ParticleState`, `SolveForcesAndVelocity`, `ScaleColor`, color/size/velocity update modules | `ParticleUpdate` |

`SpawnRate` is an **emitter** module — putting it in `ParticleUpdate` makes the system invalid, and `compile_with_results` reports only the generic `"System is invalid after compilation"` with no pointer to the culprit. If a compile is "invalid" after you added modules, **first suspect a mis-staged module** (run `list_modules` and check each `module_type`).

```python
# ✅ Minimal emitter built from scratch — note each stage:
unreal.NiagaraService.add_emitter(S, "minimal", "Sparks")
unreal.NiagaraEmitterService.add_module(S, "Sparks", "/Niagara/Modules/Spawn/Initialization/V2/InitializeParticle.InitializeParticle", "ParticleSpawn")
unreal.NiagaraEmitterService.add_module(S, "Sparks", "/Niagara/Modules/Emitter/SpawnRate.SpawnRate", "EmitterUpdate")   # NOT ParticleUpdate
unreal.NiagaraEmitterService.add_module(S, "Sparks", "/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState", "ParticleUpdate")
unreal.NiagaraEmitterService.add_module(S, "Sparks", "/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity", "ParticleUpdate")
unreal.NiagaraEmitterService.add_renderer(S, "Sparks", "SpriteRenderer", "Sprite", {})
unreal.NiagaraService.compile_with_results(S)   # expect errors=0
```

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


---

## Task Index

| Task | Sub-doc | Sample script |
|------|---------|---------------|
| Add modules / renderers to an emitter | `reference.md` | `scripts/build_emitter.pyx` |
| Set rapid-iteration params (all stages) | `reference.md` | `scripts/build_emitter.pyx` |
| Recolor an emitter (hue shift / tint / scale) | `color.md` | `scripts/recolor_emitter.pyx` |
| Manage ColorFromCurve modules | `color.md` | — |
| Build an emitter from scratch | `scratch-pad.md` | `scripts/build_emitter.pyx` |
| Author a Custom-HLSL scratch module | `scratch-pad.md` | — |

## Sub-docs

- **`color.md`** — all color editing: ColorFromCurve detection, choosing the right method
  (shift_color_hue vs tint vs scale), curve keys, per-stage color, managing ColorFromCurve.
- **`reference.md`** — module/renderer/rapid-iteration-param/graph-positioning APIs + property names.
- **`scratch-pad.md`** — building emitters from scratch and Custom-HLSL Scratch-Pad authoring.

## Verification
Compile and save after changes (`NiagaraService.compile_*` / `save`). For color edits, re-read the
curve keys or rapid-iteration params to confirm the change landed in every stage where the param exists.
