# Niagara Systems Workflows

System-level operations using NiagaraService.

---

## ⚠️ CRITICAL: Always Test Compilation

**After ANY system change**, test compilation:

```python
import unreal

path = "/Game/VFX/NS_Fire"

# Compile with detailed results
result = unreal.NiagaraService.compile_with_results(path)

if not result.success:
    print(f"❌ Compilation failed with {result.error_count} errors:")
    for error in result.errors:
        print(f"  - {error}")
    # FIX THE ERRORS, then recompile
else:
    print("✅ Compilation successful")
    # Save after successful compile
    unreal.EditorAssetLibrary.save_asset(path)
```

**Workflow:** Make change → Compile → Fix errors → Recompile → Save

---

## 1. Search → Ask User → Proceed

**ALWAYS search before creating:**

```python
import unreal

systems = unreal.NiagaraService.search_systems("/Game", "")

if len(systems) > 0:
    for s in systems:
        summary = unreal.NiagaraService.summarize(s)
        print(f"  - {summary.system_name}: {summary.emitter_names}")
    # STOP - Ask user: "Found {N} systems. Use one as template?"

# If user picks one:
unreal.AssetDiscoveryService.duplicate_asset(chosen_path, new_path)

# If user wants new:
result = unreal.NiagaraService.create_system("NS_New", "/Game/VFX")
```

---

## 2. Create System with Emitter

```python
import unreal

# Create system
result = unreal.NiagaraService.create_system("NS_Fire", "/Game/VFX")
path = result.asset_path

# Add emitter from template or minimal
templates = unreal.NiagaraService.list_emitter_templates("", "Fountain")
unreal.NiagaraService.add_emitter(
    path, 
    templates[0] if templates else "minimal", 
    "Flames"
)

# MUST compile and save
unreal.NiagaraService.compile_system(path)
unreal.EditorAssetLibrary.save_asset(path)
```

---

## 3. Duplicate & Modify Existing

```python
import unreal

# Duplicate existing system
unreal.AssetDiscoveryService.duplicate_asset(
    "/Game/VFX/NS_Fire_Template",
    "/Game/VFX/NS_Fire_Custom"
)

new_path = "/Game/VFX/NS_Fire_Custom"

# Modify user parameters
unreal.NiagaraService.set_parameter(new_path, "User.SpawnRate", "200")
unreal.NiagaraService.set_parameter(new_path, "User.Color", "(R=1,G=0,B=0,A=1)")

# Compile & save
unreal.NiagaraService.compile_system(new_path)
unreal.EditorAssetLibrary.save_asset(new_path)
```

---

## 4. Copy Emitters from Multiple Sources

```python
import unreal

# Create empty system
result = unreal.NiagaraService.create_system("NS_Combined", "/Game/VFX")
path = result.asset_path

# Copy emitters from different systems
unreal.NiagaraService.copy_emitter("/Game/VFX/NS_Fire", "Flames", path, "Fire")
unreal.NiagaraService.copy_emitter("/Game/VFX/NS_Smoke", "Smoke", path, "Smoke")

# Compile & save
unreal.NiagaraService.compile_system(path)
unreal.EditorAssetLibrary.save_asset(path)
```

---

## 5. Manage Emitters in System

```python
import unreal

path = "/Game/VFX/NS_Fire"

# List emitters in system
emitters = unreal.NiagaraService.list_emitters(path)
for e in emitters:
    print(f"{e.emitter_name}: enabled={e.is_enabled}")

# Enable/disable emitter
unreal.NiagaraService.enable_emitter(path, "Flames", True)

# Rename emitter
unreal.NiagaraService.rename_emitter(path, "Flames", "BigFlames")

# Duplicate emitter within system
unreal.NiagaraService.duplicate_emitter(path, "BigFlames", "SmallFlames")

# Remove emitter
unreal.NiagaraService.remove_emitter(path, "SmallFlames")
```

---

## 6. Working with User Parameters

```python
import unreal

path = "/Game/VFX/NS_Fire"

# List all user parameters
params = unreal.NiagaraService.list_parameters(path)
for p in params:
    print(f"{p.name}: {p.value} ({p.type_name})")

# Get specific parameter
param = unreal.NiagaraService.get_parameter(path, "User.SpawnRate")

# Set user parameter
unreal.NiagaraService.set_parameter(path, "User.SpawnRate", "100")

# Add new user parameter
unreal.NiagaraService.add_user_parameter(path, "ParticleSize", "Float", "1.0")

# Remove user parameter
unreal.NiagaraService.remove_user_parameter(path, "ParticleSize")
```

---

## 7. Spawn Niagara Actor in Level

```python
import unreal

# Get subsystems
actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# Load system asset
system = unreal.EditorAssetLibrary.load_asset("/Game/VFX/NS_Fire")

# Spawn actor
loc = unreal.Vector(0, 0, 100)
rot = unreal.Rotator(0, 0, 0)
actor = actor_subsys.spawn_actor_from_class(unreal.NiagaraActor, loc, rot)

# Assign system
actor.niagara_component.set_asset(system)

# Set instance parameters
actor.niagara_component.set_variable_float("User.SpawnRate", 150.0)

# Refresh viewports
level_subsys.editor_invalidate_viewports()
```

---

## 8. Inspect System Details

```python
import unreal

path = "/Game/VFX/NS_Fire"

# Quick summary (AI-friendly)
summary = unreal.NiagaraService.summarize(path)
print(f"Name: {summary.system_name}")
print(f"Emitters: {summary.emitter_names}")

# Detailed info
info = unreal.NiagaraService.get_system_info(path)

# Check if system exists
exists = unreal.NiagaraService.system_exists(path)
```
