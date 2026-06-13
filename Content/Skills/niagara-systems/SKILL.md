---
name: niagara-systems
display_name: Niagara Systems
description: Create and manage Niagara particle systems — system lifecycle, add/copy emitters, user parameters, and system script settings (NiagaraService). Use when the user asks to create a Niagara system, add or copy emitters into a system, add user/exposed parameters, or compile a system. For emitter internals, load niagara-emitters.
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
  - create system
  - copy system
  - system parameter
  - NiagaraService
  - NiagaraScratchPadService
  - scratch pad
  - custom hlsl
  - create_system
  - copy_system
  - System.Color
  - get_all_editable_settings
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "niagara-vfx"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

## Niagara Systems Skill

**NiagaraService** handles system-level operations:
- Create, save, compile systems
- Add/copy/remove emitters to systems
- Manage user-exposed parameters
- Read/write System script settings (SystemSpawn/SystemUpdate)
- Discover ALL editable settings across entire system
- System info and diagnostics

For **emitter-level module work** (built-in modules, renderers, rapid iteration params) load
`niagara-emitters`. For **building scratch-pad module graphs** (Custom HLSL, Map Get/Set, Op,
typed pins, wiring), use **`NiagaraScratchPadService`** — also documented in the `niagara-emitters` skill.

---

## ⚠️ Parameter Discovery Order

When looking for a parameter (e.g., color), check in this order:

1. **User Parameters** - System-level exposed parameters (User.Color, User.SpawnRate)
2. **System Script Settings** - SystemSpawn/SystemUpdate rapid iteration params (System.Color)
3. **Emitter Script Settings** - Per-emitter rapid iteration params

Use `get_all_editable_settings()` to discover ALL settings at once:

```python
import unreal

path = "/Game/VFX/NS_TeslaCoil"
settings = unreal.NiagaraService.get_all_editable_settings(path)

print(f"Total settings: {settings.total_settings_count}")

# User Parameters (system-level)
for p in settings.user_parameters:
    print(f"[User] {p.setting_path} ({p.value_type}): {p.current_value}")

# System & Emitter rapid iteration parameters  
for p in settings.rapid_iteration_parameters:
    print(f"[{p.emitter_name}/{p.script_stage}] {p.setting_path}: {p.current_value}")
```

---

## ⚠️ MANDATORY: Search Before Creating

```python
import unreal

# ALWAYS search first
systems = unreal.NiagaraService.search_systems("/Game", "")

if len(systems) > 0:
    for s in systems:
        summary = unreal.NiagaraService.summarize(s)
        print(f"  - {summary.system_name}: {summary.emitter_names}")
    # STOP - Ask user: "Found {N} systems. Use one as template?"
```

---

## Quick Reference

```python
import unreal

# === LIFECYCLE ===
result = unreal.NiagaraService.create_system("NS_Fire", "/Game/VFX")
path = result.asset_path

unreal.NiagaraService.compile_system(path)
unreal.NiagaraService.save_system(path)
unreal.NiagaraService.open_in_editor(path)

# === EMITTER MANAGEMENT ===
# Add from template
templates = unreal.NiagaraService.list_emitter_templates("", "Fountain")
unreal.NiagaraService.add_emitter(path, templates[0], "Flames")

# Add minimal empty emitter
unreal.NiagaraService.add_emitter(path, "minimal", "Sparks")

# Copy from another system
unreal.NiagaraService.copy_emitter("/Game/VFX/NS_Source", "Flames", path, "Fire")

# Duplicate within system
unreal.NiagaraService.duplicate_emitter(path, "Fire", "Fire2")

# Enable/disable
unreal.NiagaraService.enable_emitter(path, "Flames", True)

# Rename/remove
unreal.NiagaraService.rename_emitter(path, "Flames", "BigFlames")
unreal.NiagaraService.remove_emitter(path, "BigFlames")

# List emitters
emitters = unreal.NiagaraService.list_emitters(path)
for e in emitters:
    print(f"{e.emitter_name}: enabled={e.is_enabled}")

# === DISCOVER ALL SETTINGS (check this first!) ===
settings = unreal.NiagaraService.get_all_editable_settings(path)
# settings.user_parameters - User.Color, User.SpawnRate, etc.
# settings.rapid_iteration_parameters - System + Emitter script params
# Each param has: setting_path, value_type, current_value, emitter_name, script_stage

# === USER PARAMETERS (top priority) ===
params = unreal.NiagaraService.list_parameters(path)
unreal.NiagaraService.get_parameter(path, "User.SpawnRate")
unreal.NiagaraService.set_parameter(path, "User.SpawnRate", "100")
unreal.NiagaraService.add_user_parameter(path, "Color", "Color", "(R=1,G=0,B=0,A=1)")
unreal.NiagaraService.remove_user_parameter(path, "Color")

# === SYSTEM SCRIPT SETTINGS (check before emitters!) ===
# get_parameter and set_parameter now search: User → System scripts → Emitter scripts
param = unreal.NiagaraService.get_parameter(path, "System.Color")  # SystemSpawn color
unreal.NiagaraService.set_parameter(path, "System.Color", "(R=0,G=5,B=0,A=1)")

# === INFO & DIAGNOSTICS ===
summary = unreal.NiagaraService.summarize(path)
info = unreal.NiagaraService.get_system_info(path)
exists = unreal.NiagaraService.system_exists(path)
```

---

## ⚠️ User Parameter Types (`add_user_parameter`)

`add_user_parameter(path, name, type, default)` returns `True`/`False`. **The `type` string must
be one of the names below** — anything else returns `False` and logs a warning (check
`read_logs` if a call returns `False`). Do **not** guess type strings like `"array(float3)"`,
`"PositionArray"`, or `"NiagaraDataInterfaceArray"` — use the exact names/aliases here.

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
(with or without the `NiagaraDataInterface` prefix).

```python
import unreal
path = "/Game/VFX/NS_TrackPainter"

# Scalars
unreal.NiagaraService.add_user_parameter(path, "DecayRate", "Float", "0.997")
unreal.NiagaraService.add_user_parameter(path, "VolumeMin", "Vector", "(X=-5000,Y=-5000,Z=-500)")

# Data interfaces (default arg ignored)
unreal.NiagaraService.add_user_parameter(path, "StartPositions", "ArrayFloat3", "")
unreal.NiagaraService.add_user_parameter(path, "TracksGrid",     "Grid2D",      "")
unreal.NiagaraService.add_user_parameter(path, "DynamicRT",      "RenderTarget2D", "")
```

---

## Sample scripts (run via `execute_python_code`)

- **`scripts/create_system.pyx`** — create a Niagara system, add a user parameter, compile.
