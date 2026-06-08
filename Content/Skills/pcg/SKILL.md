---
name: pcg
display_name: Procedural Content Generation (PCG)
description: Create, inspect, and edit PCG Graph assets — add nodes, configure settings, connect pins, and wire procedural generation graphs (native PCG Python API). Use when the user asks to build a PCG graph, add PCG nodes (Surface Sampler, Static Mesh Spawner, filters), wire PCG pins, place a PCGVolume and generate, or set up procedural scattering.
unreal_classes:
  - PCGGraph
  - PCGNode
  - PCGEdge
  - PCGPin
  - PCGPinProperties
  - PCGSettings
  - PCGGraphFactory
  - PCGSurfaceSamplerSettings
  - PCGStaticMeshSpawnerSettings
  - PCGMeshSamplerSettings
keywords:
  - pcg
  - procedural
  - procedural content generation
  - pcg graph
  - pcg node
  - pcg pin
  - pcg edge
  - surface sampler
  - mesh spawner
  - static mesh spawner
  - point filter
  - density filter
  - transform
  - attribute
  - subgraph
  - pcg component
  - connect nodes
  - add node
  - wire
  - graph editor
---

# PCG Skill

PCG (Procedural Content Generation) in UE 5.7 is production-ready and fully scriptable via
the `PCGPythonInterop` plugin. No VibeUE service wrapper is needed — the Python API is complete.

## ⚠️ Read this before writing any code

These are the mistakes that cause infinite retry loops. Check each one before executing:

**Do not call `discover_python_class` or `discover_python_function` on any PCG class — this skill contains everything you need. Calling discover bloats context and causes content filter failures.**


1. **Volume Sampler input pin is `"Volume"`, not `"In"`** — `add_edge(input_node, 'In', sampler_node, 'Volume')`. Using `'In'` silently no-ops and produces 0 points.
2. **`voxel_size` is a `Vector`, not a float** — `set_editor_property('voxel_size', unreal.Vector(200, 200, 200))`. Passing a float raises a TypeError.
3. **`graph` property on PCGComponent is protected** — use `pcg_comp.set_graph(graph)`, never `set_editor_property('graph', ...)`.
4. **After modifying a graph, always: save → reload → reassign → generate** — skipping reload means the component runs the old cached version.
5. **Check for existing nodes before adding** — always inspect `[type(n.get_settings()).__name__ for n in graph.nodes]` first. Adding a node type that already exists creates duplicates that break wiring.
6. **Always save AND reload after every change** — call `save_asset` then `reload_packages` every time. Without the reload the PCG graph editor window won't update and users will think nothing happened.
7. **Use Volume Sampler for empty levels, not Surface Sampler** — Surface Sampler requires geometry to sample from. An empty level has no geometry, so Surface Sampler always produces 0 points. Use `PCGVolumeSamplerSettings` with `unbounded=True` for empty or sparse levels.
8. **Never spawn test actors to verify meshes or materials** — do not use `spawn_actor_from_class` to place test cubes. Verify by checking ISM instance counts on the PCGVolume instead: `sum(c.get_instance_count() for c in volume.get_components_by_class(unreal.InstancedStaticMeshComponent))`.

## ⚠️ Prerequisites

Both plugins must be enabled in the project's `.uproject`:

```json
{ "Name": "PCG", "Enabled": true },
{ "Name": "PCGPythonInterop", "Enabled": true }
```

Verify they're loaded before executing:

```python
import unreal
assert hasattr(unreal, 'PCGGraph'), "PCG plugins not enabled — check .uproject"
```

## Creating a PCG Graph Asset

```python
import unreal

factory = unreal.PCGGraphFactory()
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
graph = asset_tools.create_asset('MyPCGGraph', '/Game/PCG', unreal.PCGGraph, factory)
unreal.EditorAssetLibrary.save_asset('/Game/PCG/MyPCGGraph')
```

## Loading an Existing PCG Graph

```python
import unreal

graph = unreal.EditorAssetLibrary.load_asset('/Game/PCG/MyPCGGraph')
# graph is a PCGGraph — access nodes, input_node, output_node directly
```

Or use `manage_asset` to search first (use `action="search"`, not `action="find"`):
```
manage_asset(action="search", search_query="MyPCGGraph")
```

## Adding Nodes

Use `add_node_of_type(SettingsClass)` — returns a `(PCGNode, PCGSettings)` tuple.
All Settings classes are direct attributes of the `unreal` module — no `find_class` needed:

```python
import unreal

# Surface Sampler node
node, settings = graph.add_node_of_type(unreal.PCGSurfaceSamplerSettings)
settings.set_editor_property('points_per_squared_meter', 5.0)
settings.set_editor_property('point_extents', unreal.Vector(50, 50, 50))
node.set_node_position(200, 0)

# Static Mesh Spawner node
spawner_node, spawner_settings = graph.add_node_of_type(unreal.PCGStaticMeshSpawnerSettings)
spawner_node.set_node_position(500, 0)
```

Settings can also be read/written after creation via `node.get_settings()`:

```python
settings = node.get_settings()
settings.set_editor_property('unbounded', True)
print(settings.get_editor_property('points_per_squared_meter'))
```

## Identifying Nodes

`node.get_editor_property('node_title')` is `None` by default. Identify nodes by their settings class:

```python
for node in graph.nodes:
    print(type(node.get_settings()).__name__)
```

Note: the `nodes` array does **not** include the graph's Input and Output nodes.
Access those via `graph.get_input_node()` and `graph.get_output_node()`.

## Discovering Available Node Types

```python
import unreal
node_types = sorted([x for x in dir(unreal) if x.endswith('Settings') and 'PCG' in x])
print(node_types)
```


## Task Index

| Task | Sub-doc | Sample script |
|------|---------|---------------|
| Create / load a PCG graph | (above) | `scripts/build_pcg_graph.pyx` |
| Add / identify / discover nodes | (above) | `scripts/build_pcg_graph.pyx` |
| Connect pins / position / remove | `workflows.md` | `scripts/build_pcg_graph.pyx` |
| Place a PCGVolume and generate | `workflows.md` → Placing a PCGVolume | — |
| Surface Sampler → Mesh Spawner | `workflows.md` → Full Example | — |
| Settings property names / SMS config | `reference.md` | — |

## Sub-docs
- **`workflows.md`** — pin inspection/connection, node positioning/removal, editor notify, save/delete,
  placing a PCGVolume + generating, the full surface-sampler→mesh-spawner example, subgraphs.
- **`reference.md`** — settings property names, lowercase struct properties, Static Mesh Spawner config,
  common node settings classes.

## Sample scripts (run via `execute_python_code`)

- **`scripts/build_pcg_graph.pyx`** — create a PCG graph and add Surface Sampler + Static Mesh Spawner nodes (native PCG API).
