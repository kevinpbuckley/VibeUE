# Niagara Emitters Workflows

Emitter-level operations using NiagaraEmitterService.

---

## ⚠️ CRITICAL: Always Test Compilation After Changes

**After adding/removing modules or changing parameters**, compile and test:

```python
import unreal

path = "/Game/VFX/NS_Fire"

# After making changes to emitter
# ...

# ALWAYS compile with results to check for errors
result = unreal.NiagaraService.compile_with_results(path)

if not result.success:
    print(f"❌ Compilation failed with {result.error_count} errors:")
    for error in result.errors:
        print(f"  - {error}")
    
    # Common fixes:
    # - Missing required modules (Initialize, ParticleState, SolveForcesAndVelocity)
    # - Missing renderer
    # - Invalid parameter values
    # - Module order issues
    
    # FIX THE ERRORS, then recompile:
    result = unreal.NiagaraService.compile_with_results(path)
    
if result.success:
    print("✅ Compilation successful")
    unreal.EditorAssetLibrary.save_asset(path)
```

**Workflow:** Add module → Compile → Fix errors → Recompile → Verify → Save

---

## 1. Build Emitter from Scratch

Every emitter needs: spawn module, renderer, update modules.

```python
import unreal

path = "/Game/VFX/NS_Fire"

# Add minimal emitter
unreal.NiagaraService.add_emitter(path, "minimal", "MySparks")

# Add renderer (REQUIRED - makes particles visible)
unreal.NiagaraEmitterService.add_renderer(
    path, "MySparks",
    "SpriteRenderer",
    "Sprite",
    {}
)

# Add spawn rate module
unreal.NiagaraEmitterService.add_module(
    path, "MySparks",
    "/Niagara/Modules/Emitter/SpawnRate.SpawnRate",
    "EmitterUpdate",
    "SpawnRate"
)

# Add initialization module
unreal.NiagaraEmitterService.add_module(
    path, "MySparks",
    "/Niagara/Modules/Spawn/Initialization/InitializeParticle.InitializeParticle",
    "ParticleSpawn",
    "Initialize"
)

# Configure spawn rate
unreal.NiagaraService.set_rapid_iteration_param(
    path, "MySparks",
    "Constants.MySparks.SpawnRate.SpawnRate",
    "25"
)

# CRITICAL: Test compilation
result = unreal.NiagaraService.compile_with_results(path)
if not result.success:
    print(f"Errors: {result.errors}")
    # Fix errors and recompile
else:
    unreal.EditorAssetLibrary.save_asset(path)
unreal.NiagaraService.compile_system(path)
```

---

## 2. Configure Rapid Iteration Parameters

Rapid iteration params are module settings that can be adjusted without recompile.

```python
import unreal

path = "/Game/VFX/NS_Fire"
emitter = "Flames"

# List ALL params (shows script stage for each)
params = unreal.NiagaraService.list_rapid_iteration_params(path, emitter)
for p in params:
    print(f"[{p.script_type}] {p.parameter_name}: {p.value}")

# Set param - sets in ALL stages where it exists
# NOTE: Use full param name from list_rapid_iteration_params (e.g., "Constants.Flames.Color.Scale Color")
unreal.NiagaraService.set_rapid_iteration_param(
    path, emitter,
    "Constants.Flames.Color.Scale Color",
    "(0.0, 3.0, 0.0)"  # RGB bright green
)

# Set param in SPECIFIC stage
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    path, emitter,
    "ParticleUpdate",  # Stage name BEFORE param name
    "Constants.Flames.Color.Scale Color",
    "(0.0, 1.0, 0.0)"  # Green only in ParticleUpdate
)
```

---

## 3. Manage Modules

```python
import unreal

path = "/Game/VFX/NS_Fire"
emitter = "Flames"

# List available module scripts
scripts = unreal.NiagaraEmitterService.list_available_scripts("Color", "")

# List modules in emitter (by stage)
modules = unreal.NiagaraEmitterService.list_modules(path, emitter)
for m in modules:
    print(f"[{m.script_type}] {m.module_name}")

# Add module
unreal.NiagaraEmitterService.add_module(
    path, emitter,
    "/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity",
    "ParticleUpdate",
    "Velocity"
)

# Remove module
unreal.NiagaraEmitterService.remove_module(
    path, emitter,
    "Velocity",
    "ParticleUpdate"
)
```

---

## 4. Manage Renderers

```python
import unreal

path = "/Game/VFX/NS_Fire"
emitter = "Flames"

# List renderers
renderers = unreal.NiagaraEmitterService.list_renderers(path, emitter)

# Add sprite renderer
unreal.NiagaraEmitterService.add_renderer(
    path, emitter,
    "SpriteRenderer",
    "MySprite",
    {"Material": "/Game/Materials/M_Smoke"}
)

# Add mesh renderer
unreal.NiagaraEmitterService.add_renderer(
    path, emitter,
    "MeshRenderer",
    "MyMesh",
    {"Mesh": "/Game/Meshes/SM_Spark"}
)

# Add ribbon renderer (for trails)
unreal.NiagaraEmitterService.add_renderer(
    path, emitter,
    "RibbonRenderer",
    "MyRibbon",
    {}
)

# Add light renderer
unreal.NiagaraEmitterService.add_renderer(
    path, emitter,
    "LightRenderer",
    "MyLight",
    {}
)
```

---

## 5. Color Adjustments

### ⚠️ CRITICAL: Color Exists in BOTH ParticleSpawn AND ParticleUpdate

**Color.Scale Color** is set in BOTH stages and they multiply together:
- ParticleSpawn: Sets initial color when particle spawns
- ParticleUpdate: Multiplies color every frame

**You MUST update BOTH stages when changing color!**

```python
import unreal

path = "/Game/VFX/NS_Fire"
emitter = "Flames"

# ✅ RECOMMENDED: Use set_rapid_iteration_param (sets ALL stages automatically)
# Use full param name from list_rapid_iteration_params
green = "(0.0, 3.0, 0.0)"  # RGB values >1 make it brighter
unreal.NiagaraService.set_rapid_iteration_param(
    path, emitter,
    f"Constants.{emitter}.Color.Scale Color",
    green
)  # This sets BOTH ParticleSpawn AND ParticleUpdate!

# Also set InitializeParticle Color for initial spawn color
unreal.NiagaraService.set_rapid_iteration_param(
    path, emitter,
    f"Constants.{emitter}.InitializeParticle001.Color",
    "(0.0, 3.0, 0.0, 1.0)"  # RGBA
)

# ✅ ALTERNATIVE: Explicitly set BOTH stages
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    path, emitter,
    "ParticleSpawn",  # Stage name FIRST
    f"Constants.{emitter}.Color.Scale Color",
    "(0.0, 1.0, 0.0)"
)
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    path, emitter,
    "ParticleUpdate",
    f"Constants.{emitter}.Color.Scale Color",
    "(0.0, 1.0, 0.0)"
)

# ❌ WRONG: Only setting one stage - color may not work!
# unreal.NiagaraService.set_rapid_iteration_param_by_stage(
#     path, emitter, "ParticleUpdate", f"Constants.{emitter}.Color.Scale Color", "(0,1,0)"
# )
```

### Different Colors Per Stage (Fade Effect)

To create a fade effect (e.g., green to red), set different values:

```python
# Particles start green
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    path, emitter,
    "ParticleSpawn",
    f"Constants.{emitter}.Color.Scale Color",
    "(0.0, 1.0, 0.0)"
)

# Then fade to red over lifetime
unreal.NiagaraService.set_rapid_iteration_param_by_stage(
    path, emitter,
    "ParticleUpdate",
    f"Constants.{emitter}.Color.Scale Color",
    "(1.0, 0.0, 0.0)"
)
```

---

## 6. Verify Emitter Has Required Components

```python
import unreal

path = "/Game/VFX/NS_Fire"
emitter = "Flames"

# Check renderers
renderers = unreal.NiagaraEmitterService.list_renderers(path, emitter)
if len(renderers) == 0:
    print("⚠️ No renderer - particles won't be visible!")
    unreal.NiagaraEmitterService.add_renderer(path, emitter, "SpriteRenderer", "Sprite", {})

# Check modules
modules = unreal.NiagaraEmitterService.list_modules(path, emitter)

spawn_modules = [m for m in modules if m.script_type == "ParticleSpawn"]
update_modules = [m for m in modules if m.script_type == "ParticleUpdate"]
emitter_update = [m for m in modules if m.script_type == "EmitterUpdate"]

if len(spawn_modules) == 0:
    print("⚠️ No ParticleSpawn modules!")

if len(update_modules) == 0:
    print("⚠️ No ParticleUpdate modules!")

# Check for spawn rate
has_spawn_rate = any("spawn" in m.module_name.lower() for m in emitter_update)
if not has_spawn_rate:
    print("⚠️ No spawn rate module - no particles will spawn!")
```

---

## 7. Graph Positioning

Position emitters in the Niagara system editor graph.

```python
import unreal

path = "/Game/VFX/NS_Fire"

# Get all emitters
emitters = unreal.NiagaraService.list_emitters(path)

# Space emitters evenly (250px apart)
for i, e in enumerate(emitters):
    x = i * 250
    y = 256  # Align Y position
    unreal.NiagaraService.set_emitter_graph_position(path, e.emitter_name, x, y)

# Get specific emitter position
pos = unreal.NiagaraService.get_emitter_graph_position(path, "Flames")
print(f"Position: ({pos.x}, {pos.y})")
```
