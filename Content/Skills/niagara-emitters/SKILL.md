---
name: niagara-emitters
display_name: Niagara Emitters
description: Niagara emitter color/curve authoring (tint, hue-shift, ColorFromCurve keys), rapid-iteration parameter tuning, and Custom-HLSL scratch-pad authoring (NiagaraEmitterService + NiagaraScratchPadService). Module/renderer CRUD is owned by the engine NiagaraToolsets. Use when the user asks to recolor or hue-shift particles, edit color curves, tune emitter params, or build a scratch-pad/Custom HLSL module. For system-level lifecycle, load niagara-systems.
vibeue_classes:
  - NiagaraEmitterService
  - NiagaraScratchPadService
  - NiagaraService
unreal_classes:
  - NiagaraEmitter
  - NiagaraScript
keywords:
  - niagara emitter
  - rapid iteration
  - particle spawn
  - particle update
  - emitter update
  - niagara color
  - color from curve
  - ColorFromCurve
  - color tint
  - hue shift
  - color curve keys
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
  - set_color_tint
  - shift_color_hue
  - get_color_curve_keys
  - set_rapid_iteration_param
  - create_scratch_module
  - add_custom_hlsl_node
  - add_module_input
  - connect_pins
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "niagara-vfx"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

## Niagara Emitters Skill

> **Architecture (read first):** VibeUE extends Unreal's native MCP endpoint. Emitter **module and
> renderer CRUD** (add/remove/enable/reorder modules, add/remove renderers, set module/renderer
> inputs, search module scripts) are owned by the **engine** `NiagaraToolsets.*` toolsets, called
> through `call_tool`. VibeUE's `NiagaraEmitterService` was trimmed to the **color/curve authoring**
> the engine doesn't provide, plus a rapid-iteration-parameter reader.

**VibeUE `NiagaraEmitterService` (this skill) keeps ONLY color/curve authoring + a param reader:**
- `set_color_tint` — add/set a ScaleColor tint (handles ColorFromCurve automatically)
- `get_color_curve_keys` / `set_color_curve_keys` — read/replace ColorFromCurve keyframes
- `shift_color_hue` — hue-rotate a ColorFromCurve while preserving detail (best for recolors)
- `get_rapid_iteration_parameters` — read an emitter's rapid-iteration params

**NiagaraScratchPadService** authors scratch-pad module **graphs**:
- Create empty scratch modules on any stage
- Drop Map Get / Map Set / Op / Custom HLSL nodes
- Add typed pins, connect pins, declare module inputs/outputs
- Apply + recompile in one call

For **module / renderer CRUD** → engine `NiagaraToolsets.*` via `call_tool` (discover with
`list_toolsets` / `describe_toolset`). For **system-level** operations (create system, add emitters,
user params, compile) → engine `NiagaraToolsets.*` and the `niagara-systems` skill. Rapid-iteration
param **writes** (`set_rapid_iteration_param*`) live on **`NiagaraService`** (niagara-systems skill).

> **Loading skills:** skills load through the engine's `AgentSkillToolset` (`ListSkills`/`GetSkills`) —
> there is no `vibeue-skills-manager` tool. Run VibeUE services with `execute_python_code`
> (`unreal.<Service>.<method>()`); reach engine toolsets with `call_tool`.

---

## ⚠️ CRITICAL: Script Stages

Niagara modules exist in different **script stages**:
- `EmitterSpawn` - Runs once when emitter spawns
- `EmitterUpdate` - Runs every frame for emitter
- `ParticleSpawn` - Runs once when each particle spawns  
- `ParticleUpdate` - Runs every frame for each particle

### ⚠️ Adding modules/renderers is an engine `NiagaraToolsets` operation — stage still matters

Adding modules and renderers moved to the **engine** toolsets. Discover the exact tool + schema with
`describe_toolset` and invoke via `call_tool`:

```python
# describe_toolset("NiagaraToolsets.NiagaraToolset_System")   # (find the add-module / add-renderer tools)
# call_tool(toolset_name="NiagaraToolsets.NiagaraToolset_System",
#           tool_name="<add module tool>", arguments={ ... })
```

Whatever the engine tool's signature, **which stage a module goes in still matters** — this is the
#1 cause of a system that "compiles" but is invalid:

| Module | Correct stage |
|--------|---------------|
| `InitializeParticle` | `ParticleSpawn` |
| `SpawnRate`, `SpawnBurstInstantaneous`, `EmitterState` | `EmitterUpdate` |
| `ParticleState`, `SolveForcesAndVelocity`, `ScaleColor`, color/size/velocity update modules | `ParticleUpdate` |

`SpawnRate` is an **emitter** module — putting it in `ParticleUpdate` makes the system invalid, and
a compile reports only the generic `"System is invalid after compilation"` with no pointer to the
culprit. If a compile is "invalid" after you added modules, **first suspect a mis-staged module**
(list the emitter's modules via the engine toolset and check each module's stage). Stage strings are
`ParticleSpawn` / `ParticleUpdate` / `EmitterSpawn` / `EmitterUpdate`.

> For building an emitter entirely from a **scratch-pad / Custom HLSL** module (which IS a VibeUE
> operation), see `scratch-pad.md`.

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

| Task | Sub-doc / where |
|------|-----------------|
| Recolor an emitter (hue shift / tint / scale) | `color.md` (VibeUE) |
| Manage ColorFromCurve modules / curve keys | `color.md` (VibeUE) |
| Read rapid-iteration params | `reference.md` (VibeUE) |
| Set rapid-iteration params (all stages) | `reference.md` → `NiagaraService` (niagara-systems) |
| Add modules / renderers to an emitter | engine `NiagaraToolsets.*` via `call_tool` (see `reference.md`) |
| Build an emitter from scratch / Custom-HLSL scratch module | `scratch-pad.md` (VibeUE) |

## Sub-docs

- **`color.md`** — all color editing: ColorFromCurve detection, choosing the right method
  (shift_color_hue vs tint vs scale), curve keys, per-stage color, managing ColorFromCurve.
- **`reference.md`** — VibeUE color/curve + rapid-iteration-param readers, plus where module/renderer
  CRUD lives (engine `NiagaraToolsets`) and common property names.
- **`scratch-pad.md`** — building emitters from scratch and Custom-HLSL Scratch-Pad authoring.

## Verification
Compile and save after changes (compile via the engine `NiagaraToolsets` tool;
`unreal.EditorAssetLibrary.save_asset(path)` to persist). For color edits, re-read the curve keys
(`get_color_curve_keys`) or rapid-iteration params to confirm the change landed in every stage where
the param exists.
