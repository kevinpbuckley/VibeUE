---
name: niagara-systems
display_name: Niagara Systems
description: Rapid-iteration parameter tuning, diagnostics, and Custom-HLSL scratch-pad authoring for Niagara systems (VibeUE NiagaraService + NiagaraScratchPadService). System/emitter/parameter CRUD is owned by the engine NiagaraToolsets. Use when the user asks to tune emitter rapid-iteration params, compare/diagnose systems, or build scratch-pad/Custom HLSL modules. For emitter color/module work, load niagara-emitters.
vibeue_classes:
  - NiagaraService
  - NiagaraScratchPadService
unreal_classes:
  - EditorAssetLibrary
  - NiagaraSystem
keywords:
  - niagara system
  - NS_
  - particle system
  - vfx system
  - rapid iteration
  - System.Color
  - compare systems
  - debug activation
  - emitter lifecycle
  - NiagaraService
  - NiagaraScratchPadService
  - scratch pad
  - custom hlsl
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "niagara-vfx"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

## Niagara Systems Skill

> **Architecture (read first):** VibeUE extends Unreal's native MCP endpoint. System / emitter /
> parameter **create / add / copy / remove / list / compile** are owned by the **engine** toolset
> `NiagaraToolsets.NiagaraToolset_System` (plus `…_Assets`, `…_Component`, `…_Blueprint`, `…_Info`),
> called through `call_tool`. VibeUE's `NiagaraService` was trimmed to the rapid-iteration-parameter
> tuning + diagnostics that the engine toolset does not cover, and `NiagaraScratchPadService`
> (Custom-HLSL graph authoring) which is unique to VibeUE.

**VibeUE `NiagaraService` (this skill) keeps ONLY:**
- Rapid-iteration parameter read/write — `list_rapid_iteration_params`, `set_rapid_iteration_param`, `set_rapid_iteration_param_by_stage`
- Diagnostics — `compare_systems`, `get_emitter_lifecycle`, `debug_activation`

For **system/emitter/parameter CRUD and compile** → engine `NiagaraToolsets.*` via `call_tool`.
For **emitter color/curve authoring** → load `niagara-emitters`.
For **scratch-pad module graphs** (Custom HLSL, Map Get/Set, Op, typed pins, wiring) →
**`NiagaraScratchPadService`** — also documented in the `niagara-emitters` skill.

---

## Loading skills

Skills load through the engine's `AgentSkillToolset` (`ListSkills` / `GetSkills`) — there is no
`vibeue-skills-manager` tool. The workhorse for all VibeUE services is `execute_python_code`
(call `unreal.<Service>.<method>()` plus the full `unreal.*` Python API). Engine toolsets are
reached with `call_tool` (discover them with `list_toolsets` / `describe_toolset`).

---

## ⚠️ System / emitter / parameter CRUD → engine NiagaraToolsets

> 🛑 **CRASH RISK — Niagara user-parameter ops (issue #464).** The engine
> `NiagaraToolset_System.AddUserVariables` tool (the "add user parameter" path) can trigger a
> **fatal engine assertion** (`NiagaraVariant.cpp:108 InCount > 0`) that hard-crashes the editor —
> it builds an `FNiagaraVariant` with a zero-byte payload when the variable's type/default yields no
> value bytes. **Do NOT add Niagara user parameters via this tool until #464 is fixed.** Worse: a
> system left half-modified by a failed `AddUserVariables` is corrupted, and subsequent **read** ops
> on it (`GetUserVariables`, `GetSystemCompileState`, `GetEmitterTopology`, `GetEmitterSummary`, …)
> then crash with an array-index-out-of-bounds. If a user parameter is truly required, supply a
> concrete **non-empty** default for the exact type and verify the system still reads back before
> doing anything else. For scratch-module work, prefer the crash-safe `NiagaraScratchPadService`
> (validated solid in the sweep).

Creating a system, adding/copying/removing emitters, adding user parameters, listing emitters,
compiling — these moved to the engine. Discover and call them like this:

```python
# Discover the engine Niagara toolsets and their exact tool names + schemas:
#   list_toolsets()
#   describe_toolset("NiagaraToolsets.NiagaraToolset_System")
#   describe_toolset("NiagaraToolsets.NiagaraToolset_Assets")
#
# Then invoke a tool, e.g. create a system / add an emitter / add a user parameter:
#   call_tool(toolset_name="NiagaraToolsets.NiagaraToolset_System",
#             tool_name="<tool from describe_toolset>",
#             arguments={ ... })
```

Always inspect existing systems before creating a new one (use the engine `…_Info` / `…_Assets`
toolset to search/summarize), and ask the user before duplicating an effect as a template.

---

## ⚠️ Parameter Discovery Order (rapid-iteration tuning)

When tuning a parameter (e.g., color), VibeUE's `NiagaraService` covers the **rapid-iteration
parameters** on emitter scripts. User-exposed (`User.*`) and system-script (`System.*`) parameters
are read/written through the engine `NiagaraToolsets` parameter tools.

1. **User Parameters** (`User.Color`, `User.SpawnRate`) → engine `NiagaraToolsets`
2. **System Script Settings** (`System.Color`, SystemSpawn/Update) → engine `NiagaraToolsets`
3. **Emitter Script Settings** (per-emitter rapid-iteration params) → VibeUE `NiagaraService` below

```python
import unreal

path = "/Game/VFX/NS_TeslaCoil"

# Emitter rapid-iteration params (VibeUE):
params = unreal.NiagaraService.list_rapid_iteration_params(path, "Sparks")
for p in params:
    print(f"[{p.script_type}] {p.parameter_name}: {p.value}")
```

---

## Quick Reference (VibeUE NiagaraService — kept methods)

```python
import unreal

path = "/Game/VFX/NS_Fire"

# === RAPID ITERATION PARAMETERS (emitter script settings) ===
# Discover the exact param names:
params = unreal.NiagaraService.list_rapid_iteration_params(path, "Flames")
for p in params:
    print(f"[{p.script_type}] {p.parameter_name}: {p.value}")

# Set ALL matching stages at once (recommended for color):
unreal.NiagaraService.set_rapid_iteration_param(
    path, "Flames", "Constants.flames.Color.Scale Color", "(0.0, 3.0, 0.0)")

# Set one specific stage when the param exists in several:
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    path, "Flames", "ParticleUpdate", "Constants.flames.Color.Scale Color", "(0.0, 3.0, 0.0)")

# === DIAGNOSTICS ===
cmp = unreal.NiagaraService.compare_systems("/Game/VFX/NS_Fire", "/Game/VFX/NS_Fire_Copy")
print(f"Differences: {cmp.difference_count}")
for d in cmp.differences:
    print(f"  {d.property_name}: {d.source_value} vs {d.target_value}")

info = unreal.NiagaraService.get_emitter_lifecycle(path, "Flames")  # struct or None
print(info.loop_behavior, info.life_cycle_mode)

print(unreal.NiagaraService.debug_activation(path))  # why isn't this system playing?
```

System lifecycle, user parameters, and compile are NOT on `NiagaraService` anymore — use the
engine `NiagaraToolsets.*` tools shown above. After tuning, compile via the engine toolset (or
`unreal.EditorAssetLibrary.save_asset(path)` to persist).

---

## ⚠️ User Parameter Types (engine `NiagaraToolsets` — reference)

Adding a user-exposed parameter is now an engine `NiagaraToolsets` operation (run
`describe_toolset` for its exact argument schema). The type names below are the canonical Niagara
types — keep them as a reference when supplying a type argument; do **not** guess strings like
`"array(float3)"` or `"NiagaraDataInterfaceArray"`.

**Scalar / struct types** (default value is parsed from the string):

| `type`                         | Niagara type | Example default            |
|--------------------------------|--------------|----------------------------|
| `Float`                        | float        | `"0.997"`                  |
| `Int` (or `Int32`)             | int          | `"200"`                    |
| `Bool`                         | bool         | `"true"`                   |
| `Vector` (or `Vector3`)        | Vector       | `"(X=-5000,Y=-5000,Z=-500)"` |
| `Vector2`                      | Vector2D     | `"(X=0,Y=0)"`              |
| `Vector4`                      | Vector4      | `"(X=0,Y=0,Z=0,W=1)"`      |
| `Color` (or `LinearColor`)     | LinearColor  | `"(R=1,G=0,B=0,A=1)"`      |

**Data interface types** — the values the game/Blueprint writes each frame (typed arrays,
grids, render targets). Pass the alias or the full `NiagaraDataInterface…` class name; the
`default` argument is ignored (a default DI instance is allocated automatically):

| Alias (case-insensitive)                 | Data interface class                     |
|------------------------------------------|------------------------------------------|
| `ArrayFloat3` / `ArrayVector` / `VectorArray` | `NiagaraDataInterfaceArrayFloat3`   |
| `ArrayPosition` / `PositionArray`        | `NiagaraDataInterfaceArrayPosition`      |
| `ArrayFloat` / `FloatArray`              | `NiagaraDataInterfaceArrayFloat`         |
| `ArrayFloat2`, `ArrayFloat4`             | `NiagaraDataInterfaceArrayFloat2/4`      |
| `ArrayInt` / `ArrayInt32`                | `NiagaraDataInterfaceArrayInt32`         |
| `ArrayBool`, `ArrayColor`, `ArrayQuat`   | matching `NiagaraDataInterfaceArray…`    |
| `Grid2D` / `Grid2DCollection`            | `NiagaraDataInterfaceGrid2DCollection`   |
| `Grid3D` / `Grid3DCollection`            | `NiagaraDataInterfaceGrid3DCollection`   |
| `RenderTarget2D` / `TextureRenderTarget` | `NiagaraDataInterfaceRenderTarget2D`     |

Any other concrete `UNiagaraDataInterface` subclass also works if you pass its exact class name
(with or without the `NiagaraDataInterface` prefix). Supply the type/default arguments to the
engine `NiagaraToolsets` "add user parameter" tool (see `describe_toolset` for the exact field
names); data-interface types ignore the default value (a default DI instance is allocated).

---

## Related

- **niagara-emitters** — color/curve authoring + scratch-pad Custom HLSL.
- Engine `NiagaraToolsets.*` (via `call_tool`) — system/emitter/parameter/renderer CRUD and compile.
