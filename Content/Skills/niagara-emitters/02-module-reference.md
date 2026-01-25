# Niagara Module Reference

Common built-in module scripts and their purposes.

---

## Emitter Requirements Checklist

**Every working emitter MUST have:**

| Requirement | What to Add | Why |
|-------------|-------------|-----|
| **Renderer** | `add_renderer(path, emitter, "SpriteRenderer", ...)` | Makes particles visible |
| **Initialize** | `InitializeParticle` module | Sets up particle attributes |
| **Spawn Rate** | `SpawnRate` module in EmitterUpdate | Controls particles/second |
| **Particle State** | `ParticleState` module | Manages lifetime |
| **Forces** | `SolveForcesAndVelocity` module | Applies physics |

---

## Script Stages

| Stage | Runs When | Use For |
|-------|-----------|---------|
| `EmitterSpawn` | Once when emitter starts | One-time setup |
| `EmitterUpdate` | Every frame for emitter | Spawn rate, emitter state |
| `ParticleSpawn` | Once when each particle spawns | Initial velocity, position, color |
| `ParticleUpdate` | Every frame for each particle | Movement, color changes, size over life |

---

## Spawn Modules (ParticleSpawn Stage)

| Path | Description |
|------|-------------|
| `/Niagara/Modules/Spawn/Initialization/InitializeParticle` | Core particle initialization |
| `/Niagara/Modules/Spawn/Location/SpawnBurst` | Spawn particles in a burst |
| `/Niagara/Modules/Spawn/Location/SpawnPerUnit` | Spawn based on distance traveled |
| `/Niagara/Modules/Spawn/Location/BoxLocation` | Spawn in box area |
| `/Niagara/Modules/Spawn/Location/SphereLocation` | Spawn in sphere |
| `/Niagara/Modules/Spawn/Velocity/AddVelocityInCone` | Initial velocity in cone |
| `/Niagara/Modules/Spawn/Velocity/AddVelocityFromPoint` | Velocity away from a point |
| `/Niagara/Modules/Spawn/Velocity/AddVelocity` | Direct velocity |

---

## Emitter Update Modules

| Path | Description |
|------|-------------|
| `/Niagara/Modules/Emitter/SpawnRate.SpawnRate` | Controls spawn rate |
| `/Niagara/Modules/Emitter/SpawnBurstInstantaneous` | Spawn N particles immediately |
| `/Niagara/Modules/Emitter/EmitterState` | Emitter lifecycle management |

---

## Particle Update Modules

### Core (Usually Required)
| Path | Description |
|------|-------------|
| `/Niagara/Modules/Update/Lifetime/ParticleState` | Manages particle lifetime |
| `/Niagara/Modules/Solvers/SolveForcesAndVelocity` | Applies all forces |

### Physics/Movement
| Path | Description |
|------|-------------|
| `/Niagara/Modules/Update/Acceleration/Gravity` | Apply gravity force |
| `/Niagara/Modules/Update/Acceleration/Drag` | Apply drag/friction |
| `/Niagara/Modules/Update/Forces/PointAttraction` | Attract to a point |
| `/Niagara/Modules/Update/Forces/Vortex` | Spin around an axis |
| `/Niagara/Modules/Update/Forces/CurlNoiseForce` | Turbulent movement |

### Visual
| Path | Description |
|------|-------------|
| `/Niagara/Modules/Update/Color/ColorFromCurve` | Color over lifetime |
| `/Niagara/Modules/Update/Color/ColorByLife` | Change color over lifetime |
| `/Niagara/Modules/Update/Color/ColorBySpeed` | Change color based on speed |
| `/Niagara/Modules/Update/Color/ScaleColor` | Multiply color values |
| `/Niagara/Modules/Update/Size/ScaleSpriteSize` | Scale sprite size |
| `/Niagara/Modules/Update/Size/ScaleSpriteBySpeed` | Scale based on speed |
| `/Niagara/Modules/Update/Size/ScaleMeshSize` | Scale mesh size |
| `/Niagara/Modules/Update/Size/FloatFromCurve` | Animate float values |

---

## Renderer Types

| Type | Class | Description |
|------|-------|-------------|
| `SpriteRenderer` | `NiagaraSpriteRendererProperties` | 2D billboards |
| `MeshRenderer` | `NiagaraMeshRendererProperties` | 3D meshes |
| `RibbonRenderer` | `NiagaraRibbonRendererProperties` | Trails/ribbons |
| `LightRenderer` | `NiagaraLightRendererProperties` | Dynamic lights |
| `ComponentRenderer` | `NiagaraComponentRendererProperties` | Actor components |

---

## Common Rapid Iteration Parameters

**NOTE:** Use full parameter names from `list_rapid_iteration_params`. Format: `Constants.<emitter>.<module>.<property>`

| Property | Type | Stage(s) | Example Value |
|----------|------|----------|---------------|
| `Constants.<e>.SpawnRate.SpawnRate` | Float | EmitterUpdate | `"50"` |
| `Constants.<e>.SpawnBurst.SpawnCount` | Int | EmitterSpawn | `"100"` |
| `Constants.<e>.Color.Scale Color` | Vector3 | ParticleSpawn, ParticleUpdate | `"(0.0, 1.0, 0.0)"` |
| `Constants.<e>.InitializeParticle001.Color` | Color | ParticleSpawn | `"(0.0, 1.0, 0.0, 1.0)"` |
| `Constants.<e>.Lifetime.Lifetime` | Float | ParticleSpawn | `"2.0"` |
| `Constants.<e>.Velocity.Velocity` | Vector | ParticleSpawn | `"(0.0, 0.0, 100.0)"` |

---

## Parameter Type Mapping

When setting parameters use numeric tuple format:

| Type String | Niagara Type | Value Format |
|-------------|--------------|--------------|
| `Float` | Float | `"1.5"` |
| `Int` | Int32 | `"100"` |
| `Bool` | Boolean | `"true"` or `"false"` |
| `Vector3f` | Vector3 (RGB) | `"(0.0, 1.0, 0.0)"` |
| `Vector2` | Vector2 | `"(64.0, 64.0)"` |
| `Vector` | Vector3 | `"(0.0, 0.0, 100.0)"` |
| `LinearColor` | Color (RGBA) | `"(0.0, 1.0, 0.0, 1.0)"` | |

---

## Simulation Targets

Emitters can run on CPU or GPU:

| SimTarget | Description |
|-----------|-------------|
| `CPUSim` | Runs on CPU, more flexible |
| `GPUComputeSim` | Runs on GPU, higher particle counts |

Check with:
```python
emitters = unreal.NiagaraService.list_emitters(path)
for e in emitters:
    print(f"{e.emitter_name}: {e.sim_target}")
```
