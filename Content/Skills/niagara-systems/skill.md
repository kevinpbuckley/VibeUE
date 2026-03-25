---
name: niagara-systems
display_name: Niagara Systems
description: Create and manage Niagara particle systems - system lifecycle, adding/copying emitters, user parameters, system script settings
vibeue_classes:
  - NiagaraService
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
  - create_system
  - copy_system
  - System.Color
  - get_all_editable_settings
---

## Niagara Systems Skill

**NiagaraService** handles system-level operations:
- Create, save, compile systems
- Add/copy/remove emitters to systems
- Manage user-exposed parameters
- Read/write System script settings (SystemSpawn/SystemUpdate)
- Discover ALL editable settings across entire system
- System info and diagnostics

For **emitter-level** operations (modules, renderers, internal params), load the `niagara-emitters` skill.

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
