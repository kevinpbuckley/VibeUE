---
name: niagara-emitters/reference
description: Niagara emitter quick reference — VibeUE rapid-iteration-parameter readers/writers, where module/renderer CRUD lives (engine NiagaraToolsets), and common property names.
---

## Where each operation lives

| Operation | API |
|-----------|-----|
| Add / remove / enable / reorder **modules** | engine `NiagaraToolsets.*` via `call_tool` |
| Set / get **module inputs** | engine `NiagaraToolsets.*` |
| Add / remove / enable **renderers**, set renderer properties | engine `NiagaraToolsets.*` |
| Search module scripts / list built-in modules | engine `NiagaraToolsets.*` |
| **Read** rapid-iteration params | VibeUE `NiagaraService.list_rapid_iteration_params` / `NiagaraEmitterService.get_rapid_iteration_parameters` |
| **Write** rapid-iteration params | VibeUE `NiagaraService.set_rapid_iteration_param[_by_stage]` |
| Color / curve authoring | VibeUE `NiagaraEmitterService` (see `color.md`) |
| Scratch-pad / Custom HLSL graphs | VibeUE `NiagaraScratchPadService` (see `scratch-pad.md`) |

## Modules & renderers → engine NiagaraToolsets

```python
# Discover the exact tool names + input schemas, then call them:
#   list_toolsets()
#   describe_toolset("NiagaraToolsets.NiagaraToolset_System")
#   call_tool(toolset_name="NiagaraToolsets.NiagaraToolset_System",
#             tool_name="<add/remove/enable module or renderer tool>",
#             arguments={ ... })
#
# Stage still matters when adding modules (ParticleSpawn / ParticleUpdate /
# EmitterSpawn / EmitterUpdate) — see SKILL.md for which module goes in which stage.
```

## Rapid-iteration parameters (VibeUE)

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"
emitter_name = "YourEmitter"

# === READ ===
# List all params (shows which stage they're in)
params = unreal.NiagaraService.list_rapid_iteration_params(system_path, emitter_name)
for p in params:
    print(f"[{p.script_type}] {p.parameter_name}: {p.value}")

# OR NiagaraEmitterService.get_rapid_iteration_parameters for more detail
params = unreal.NiagaraEmitterService.get_rapid_iteration_parameters(system_path, emitter_name)
for p in params:
    print(f"[{p.input_type}] {p.input_name}: {p.current_value}")

# === WRITE ===
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
```

> Graph positioning (reordering emitters in the Niagara editor graph) is an engine
> `NiagaraToolsets` operation — discover it with `describe_toolset`.

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

