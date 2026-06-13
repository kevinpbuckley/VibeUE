---
name: niagara-emitters/reference
description: Niagara emitter quick reference — module/renderer/rapid-iteration-parameter/graph-positioning APIs and common property names.
---

## Quick Reference

```python
import unreal

# Generic variable pattern - replace with your actual paths
system_path = "/Game/Path/To/NS_YourSystem"
emitter_name = "YourEmitter"

# === MODULES ===
# Find module scripts by keyword (NOT "list_available_scripts" — that does not exist)
scripts = unreal.NiagaraEmitterService.search_module_scripts("Color")
# ...or list the common built-ins:
builtins = unreal.NiagaraEmitterService.list_builtin_modules()

# Add module to emitter — exactly 4 args; the 4th is the stage and must be one of
# ParticleSpawn / ParticleUpdate / EmitterSpawn / EmitterUpdate (anything else returns False).
# See SKILL.md "add_module stage string must be EXACT" for which module goes in which stage.
ok = unreal.NiagaraEmitterService.add_module(
    system_path, emitter_name,
    "/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity",
    "ParticleUpdate"
)
assert ok, "add_module returned False — check the stage string and module path"

# List modules in emitter. Struct fields: module_name, module_type (the stage),
# module_index, is_enabled, script_asset_path.
modules = unreal.NiagaraEmitterService.list_modules(system_path, emitter_name)
for m in modules:
    print(f"[{m.module_type}] {m.module_name}")

# === RENDERERS ===
# Add sprite renderer
unreal.NiagaraEmitterService.add_renderer(
    system_path, emitter_name,
    "SpriteRenderer",
    "MySprite",
    {"Material": "/Game/Materials/M_Smoke"}
)

# Renderer types: SpriteRenderer, RibbonRenderer, MeshRenderer, LightRenderer

# === RAPID ITERATION PARAMETERS ===
# List all params (shows which stage they're in)
params = unreal.NiagaraService.list_rapid_iteration_params(system_path, emitter_name)
for p in params:
    print(f"[{p.script_type}] {p.parameter_name}: {p.value}")

# OR use NiagaraEmitterService.get_rapid_iteration_parameters for more detail
params = unreal.NiagaraEmitterService.get_rapid_iteration_parameters(system_path, emitter_name)
for p in params:
    print(f"[{p.script_type}] {p.input_name}: {p.current_value}")

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

# === GRAPH POSITIONING ===
# Move emitter in Niagara editor graph
pos = unreal.NiagaraService.get_emitter_graph_position(system_path, emitter_name)
unreal.NiagaraService.set_emitter_graph_position(system_path, emitter_name, pos.x + 250, pos.y)
```

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

