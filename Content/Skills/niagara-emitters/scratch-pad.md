---
name: niagara-emitters/scratch-pad
description: Building Niagara emitters from scratch and Scratch-Pad authoring (NiagaraScratchPadService) — Custom HLSL modules, typed pins, Map Get/Set wiring, supported pin types, op nodes, and gotchas.
---

## Building Emitters from Scratch

Minimal emitter needs:
1. **Spawn module** (one of): Spawn Rate (continuous) or Spawn Burst Instantaneous (one-shot)
2. **Renderer** (SpriteRenderer minimum)
3. **Particle Update** modules for behavior

> **Creating the emitter and adding its built-in modules/renderer is an engine `NiagaraToolsets`
> operation** (create system / add emitter / add module / add renderer / compile). Discover the tools
> with `describe_toolset("NiagaraToolsets.NiagaraToolset_System")` and call via `call_tool`. Stage
> still matters: **SpawnRate is an EMITTER module → `EmitterUpdate`, NOT `ParticleUpdate`** (a
> mis-staged SpawnRate compiles to an INVALID system).

Once the emitter + modules exist, configure spawn rate with the VibeUE rapid-iteration write. Use the
FULL param name (from `list_rapid_iteration_params`), not a bare `"SpawnRate"`:

```python
import unreal

system_path = "/Game/Path/To/NS_YourSystem"

unreal.NiagaraService.set_rapid_iteration_param(
    system_path, "MySparks",
    "Constants.MySparks.SpawnRate.SpawnRate", "25"
)
# Then compile via the engine NiagaraToolsets tool.
```

The rest of THIS doc — the scratch-pad / Custom HLSL graph authoring — is the VibeUE
`NiagaraScratchPadService` and is fully supported here.

---

## Scratch-Pad Authoring (NiagaraScratchPadService)

A **scratch-pad module** is a user-authored UNiagaraScript stored inside an emitter (or system) rather than being a shared asset. It is where you build custom per-particle logic — Map Get reads + math + Custom HLSL + Map Set writes — for one specific effect. `NiagaraEmitterService` can only see scratch modules as opaque "Custom" function-call nodes; **`NiagaraScratchPadService` is the API that reaches inside them and builds the graph**.

### When to use the Scratch Pad service

| Task | Use |
|------|-----|
| Add a built-in module from `/Niagara/Modules/...` | engine `NiagaraToolsets.*` (via `call_tool`) |
| Set a value on an existing module input | engine `NiagaraToolsets.*` |
| Build per-particle math / splat logic in HLSL | **`NiagaraScratchPadService`** (VibeUE) |
| Wire a Custom HLSL node with typed inputs/outputs | **`NiagaraScratchPadService`** (VibeUE) |
| Add Map Get / Map Set / Op / If nodes | **`NiagaraScratchPadService`** (VibeUE) |

### The pipeline (mental model)

```
Emitter Stage Stack:  [Spawn]→[Update]→...→[YOUR SCRATCH MODULE]→[Output]
                                              │
Scratch graph inside that module:
   [Map In] → [Map Get  Module.X] → [Custom HLSL / Op nodes] → [Map Set  Output.Y] → [Map Out]
```

The default module template (used by `create_scratch_module`) already wires Map In → Map Get → Map Set → Map Out for you. You add nodes between Map Get and Map Set and connect them.

### End-to-end: build a Custom-HLSL splat module

```python
import unreal

S   = "/Game/VFX/NS_TrackPainter"
E   = "CompletelyEmpty"

# 1. Create the empty scratch module on the Particle Update stage
r = unreal.NiagaraScratchPadService.create_scratch_module(S, E, "ParticleUpdate", "SplatLine")
assert r.success, r.message   # result field is `.success` (NOT `.b_success`)
MOD = r.module_name           # e.g. "SplatLine"
print("script path:", r.script_path)

# 2. Declare the module inputs (becomes "Module.X" pins on the Map Get node, and
#    exposes 'X' as a stack input in the emitter)
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "StartPositions", "ArrayVector")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "EndPositions",   "ArrayVector")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "TireRadius",     "float")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "VolumeMin",      "Vector")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "VolumeMax",      "Vector")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "DecayRate",      "float")
unreal.NiagaraScratchPadService.add_module_input(S, E, MOD, "TracksGrid",     "Grid2D")

# 3. Drop a Custom HLSL node with the splat math
hlsl_code = """
// Inputs (variable names match pin names)
int Idx = ExecutionIndex;
Float3 A = StartPositions.Get(Idx);
Float3 B = EndPositions.Get(Idx);
Float3 Dir = normalize(B - A);
// ... your line-rasterization splat code that writes into TracksGrid ...
Splatted = 1;
"""
node = unreal.NiagaraScratchPadService.add_custom_hlsl_node(S, E, MOD, hlsl_code, 200, 0)
HLSL = node.node_id

# 4. Declare typed pins on the Custom HLSL node so it can take inputs and write outputs
for n,t in [("StartPositions","ArrayVector"),("EndPositions","ArrayVector"),
            ("TireRadius","float"),("VolumeMin","Vector"),("VolumeMax","Vector"),
            ("DecayRate","float"),("TracksGrid","Grid2D")]:
    unreal.NiagaraScratchPadService.add_pin(S, E, MOD, HLSL, "Input",  t, n)
unreal.NiagaraScratchPadService.add_pin(S, E, MOD, HLSL, "Output", "bool", "Splatted")

# 5. Wire Map Get → Custom HLSL → Map Set
# (find the Map Get / Map Set IDs from the freshly populated graph)
nodes = unreal.NiagaraScratchPadService.list_nodes(S, E, MOD)
mapget = next(n for n in nodes if n.node_type == "MapGet").node_id
mapset = next(n for n in nodes if n.node_type == "MapSet").node_id

for v in ["StartPositions","EndPositions","TireRadius","VolumeMin","VolumeMax","DecayRate","TracksGrid"]:
    unreal.NiagaraScratchPadService.connect_pins(S, E, MOD, mapget, f"Module.{v}", HLSL, v)

unreal.NiagaraScratchPadService.add_module_output(S, E, MOD, "Particles.Splatted", "bool")
unreal.NiagaraScratchPadService.connect_pins(S, E, MOD, HLSL, "Splatted", mapset, "Particles.Splatted")

# 6. Apply + recompile + save (one call)
unreal.NiagaraScratchPadService.apply_changes(S)
```

### `add_module_input` / `add_module_output` are idempotent — call once per name

Each `add_module_input(name)` adds one `Module.<name>` output pin to the Map Get; each
`add_module_output(name)` adds one `<name>` input pin (e.g. `Particles.Splatted`) to the Map Set.
**Calling either twice for the same name is a no-op now** — the existing pin is reused, not
duplicated. (Older builds appended a duplicate pin every call; two same-named *connected* writes
on the Map Set produce duplicate attribute writes and the compiler then rejects the system with
`"System is invalid after compilation"`.) So: declare each input/output exactly once, and if you
are bisecting a compile failure, do **not** re-add outputs — inspect with `get_node_pins` instead.

### Supported pin types (TypeName strings)

| String | Niagara type |
|--------|--------------|
| `float`, `int`, `bool` | Scalars |
| `Vector`/`Vec3`, `Vector2`/`Vec2`, `Vector4`/`Vec4` | Vectors |
| `Position`, `Color`/`LinearColor` | Special |
| `ArrayFloat`, `ArrayVector`/`ArrayFloat3`, `ArrayFloat2`, `ArrayFloat4`, `ArrayInt`, `ArrayPosition` | DI arrays |
| `Grid2D`/`Grid2DCollection` | Grid 2D Collection DI |
| `RenderTarget2D`/`DynamicRT` | Render Target 2D DI |

### Op node names (`add_op_node`)

Niagara ops live in the `Numeric::` namespace and the service auto-prefixes bare names:

| Pass | Resolves to |
|------|-------------|
| `Add`, `Multiply`, `Subtract`, `Divide` | `Numeric::Add` … |
| `Dot`, `Cross`, `Normalize`, `Length` | `Numeric::Dot` … |
| `Lerp`, `Clamp`, `Min`, `Max`, `Saturate` | `Numeric::Lerp` … |
| `Sin`, `Cos`, `Tan`, `ASin`, `ACos` | `Numeric::Sin` … |

If a bare name doesn't resolve, pass the full `Namespace::Op` string.

### Niagara Custom HLSL gotchas (NOT plain HLSL)

- Reference pin variables by their **pin name** verbatim (`StartPositions`, not `_in_StartPositions`).
- Use **`Float3`**, **`Float2`**, **`Float4`** (capitalized) for vector types in the HLSL body, not `float3`.
- Array DI calls use the generated function names, e.g. `StartPositions.Get(Index)` returns a `Float3`.
- Grid 2D writes use `TracksGrid.SetVector4Value(AttributeIndex, X, Y, Value)` style — see the Niagara docs for the exact signatures the translator generates.
- For pure node-based logic without HLSL, use `add_op_node` + `connect_pins` between Map Get / Map Set.

> ⚠️ **`Grid2DCollection`, `Grid3DCollection`, and `RenderTarget2D` data interfaces are GPU-compute
> only.** If their inputs/writes appear in an emitter whose Sim Target is **CPU** (the default for a
> `minimal` emitter), `compile_with_results` returns `success=False` with the generic message
> `"System is invalid after compilation"` — this is a Niagara constraint, not a service failure. Set
> the emitter to **GPUComputeSim** before wiring grid/render-target logic. A Custom HLSL bool output
> wired to a plain `Particles.*` attribute compiles fine on CPU; it's the grid/RT data interfaces that
> force GPU. When you hit "System is invalid", first check the emitter's Sim Target.

### One pattern that fixes most "input not found" errors

Module input names are namespaced: a stack input shown as `GridWidth` is actually the parameter
`Module.GridWidth`. When a set-module-input call (engine `NiagaraToolsets`) returns NOT FOUND for a
bare name, try the `Module.` / `Output.` / `Particles.` prefix. Inside a scratch graph, the pin name
on the Map Get node is likewise `Module.<Name>` — list pins with `get_node_pins` to confirm.

### What `apply_changes` does

A single call:
1. Walks every stack module across every emitter and calls `RefreshFromExternalChanges()` on each one referencing a scratch script (so new module inputs/outputs propagate to the stack UI).
2. Calls `FNiagaraStackGraphUtilities::RebuildEmitterNodes` to rebuild the system's compiled emitter nodes.
3. Marks dirty, recompiles, waits, saves.

Call it **once** at the end of a batch of graph edits — not after every node creation.

### Discovery

```python
import unreal
methods = unreal.SystemLibrary.get_function_names(unreal.NiagaraScratchPadService.static_class())
# or via VibeUE:
print(unreal.discover_python_class("unreal.NiagaraScratchPadService"))
```
