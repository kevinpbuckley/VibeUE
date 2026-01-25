---
name: niagara-systems
display_name: Niagara Systems
description: Create and manage Niagara particle systems - system lifecycle, adding/copying emitters, user parameters
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
---

## Niagara Systems Skill

**NiagaraService** handles system-level operations:
- Create, save, compile systems
- Add/copy/remove emitters to systems
- Manage user-exposed parameters
- System info and diagnostics

For **emitter-level** operations (modules, renderers, internal params), load the `niagara-emitters` skill.

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

# === USER PARAMETERS ===
params = unreal.NiagaraService.list_parameters(path)
unreal.NiagaraService.get_parameter(path, "User.SpawnRate")
unreal.NiagaraService.set_parameter(path, "User.SpawnRate", "100")
unreal.NiagaraService.add_user_parameter(path, "Color", "Color", "(R=1,G=0,B=0,A=1)")
unreal.NiagaraService.remove_user_parameter(path, "Color")

# === INFO & DIAGNOSTICS ===
summary = unreal.NiagaraService.summarize(path)
info = unreal.NiagaraService.get_system_info(path)
exists = unreal.NiagaraService.system_exists(path)
```
