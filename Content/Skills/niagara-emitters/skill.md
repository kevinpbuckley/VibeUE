---
name: niagara-emitters
display_name: Niagara Emitters
description: Configure Niagara emitter internals - modules, renderers, rapid iteration parameters, graph positioning
vibeue_classes:
  - NiagaraEmitterService
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
  - NiagaraEmitterService
  - add_module
  - add_renderer
  - list_modules
  - set_rapid_iteration_param
---

## Niagara Emitters Skill

**NiagaraEmitterService** handles emitter-level operations:
- Add/configure modules (spawn, update, render)
- Add/configure renderers (sprite, ribbon, mesh)
- Read/write rapid iteration parameters
- Graph positioning for emitters

For **system-level** operations (create, add emitters, user params), load the `niagara-systems` skill.

---

## ⚠️ CRITICAL: Script Stages

Niagara modules exist in different **script stages**:
- `EmitterSpawn` - Runs once when emitter spawns
- `EmitterUpdate` - Runs every frame for emitter
- `ParticleSpawn` - Runs once when each particle spawns  
- `ParticleUpdate` - Runs every frame for each particle

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

### ColorFromCurve Warning

Some emitters have a **ColorFromCurve** module that overrides color with a curve over lifetime. Check for it:
```python
modules = unreal.NiagaraEmitterService.list_modules(path, emitter)
curve_mods = [m for m in modules if "ColorFromCurve" in m.module_name]
if curve_mods:
    print("ColorFromCurve found - may override your color settings")
```

---

## Quick Reference

```python
import unreal

path = "/Game/VFX/NS_Fire"
emitter = "Flames"

# === MODULES ===
# List available module scripts
scripts = unreal.NiagaraEmitterService.list_available_scripts("Sprite", "")

# Add module to emitter
unreal.NiagaraEmitterService.add_module(
    path, emitter,
    "/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity",
    "ParticleUpdate",
    "Velocity"
)

# List modules in emitter
modules = unreal.NiagaraEmitterService.list_modules(path, emitter)
for m in modules:
    print(f"{m.script_type}: {m.module_name}")

# === RENDERERS ===
# Add sprite renderer
unreal.NiagaraEmitterService.add_renderer(
    path, emitter,
    "SpriteRenderer",
    "MySprite",
    {"Material": "/Game/Materials/M_Smoke"}
)

# Renderer types: SpriteRenderer, RibbonRenderer, MeshRenderer, LightRenderer

# === RAPID ITERATION PARAMETERS ===
# List all params (shows which stage they're in)
params = unreal.NiagaraService.list_rapid_iteration_params(path, emitter)
for p in params:
    print(f"[{p.script_type}] {p.parameter_name}: {p.value}")

# OR use NiagaraEmitterService.get_rapid_iteration_parameters for more detail
params = unreal.NiagaraEmitterService.get_rapid_iteration_parameters(path, emitter)
for p in params:
    print(f"[{p.script_type}] {p.input_name}: {p.current_value}")

# Set param in ALL matching stages (recommended for color)
# Use full param name from list output (e.g., "Constants.Flames.Color.Scale Color")
unreal.NiagaraService.set_rapid_iteration_param(
    path, emitter,
    f"Constants.{emitter}.Color.Scale Color",
    "(0.0, 3.0, 0.0)"  # RGB - bright green
)

# Set param in SPECIFIC stage (for precise control)
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    path, emitter,
    "ParticleUpdate",  # Stage name comes BEFORE param name
    f"Constants.{emitter}.Color.Scale Color",
    "(0.0, 3.0, 0.0)"
)

# === GRAPH POSITIONING ===
# Move emitter in Niagara editor graph
pos = unreal.NiagaraService.get_emitter_graph_position(path, emitter)
unreal.NiagaraService.set_emitter_graph_position(path, emitter, pos.x + 250, pos.y)
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

## Building Emitters from Scratch

Minimal emitter needs:
1. **Spawn module** (one of):
   - Spawn Rate for continuous
   - Spawn Burst Instantaneous for one-shot
2. **Renderer** (SpriteRenderer minimum)
3. **Particle Update** modules for behavior

```python
import unreal

# Create empty emitter
unreal.NiagaraService.add_emitter(path, "minimal", "MySparks")

# Add spawn
unreal.NiagaraEmitterService.add_module(
    path, "MySparks",
    "/Niagara/Modules/Emitter/SpawnRate.SpawnRate",
    "EmitterUpdate", "SpawnRate"
)

# Add renderer
unreal.NiagaraEmitterService.add_renderer(
    path, "MySparks",
    "SpriteRenderer", "Sprite", {}
)

# Configure spawn rate
unreal.NiagaraEmitterService.set_rapid_iteration_param(
    path, "MySparks",
    "SpawnRate", "25"
)

# Compile to apply
unreal.NiagaraService.compile_system(path)
```
