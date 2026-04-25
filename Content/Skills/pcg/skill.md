---
name: pcg
display_name: Procedural Content Generation (PCG)
description: Create, inspect, and edit PCG Graph assets — add nodes, configure settings, connect pins, and wire up procedural generation graphs
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

## Inspecting Pin Labels

Pin labels live on `PCGPinProperties`, not directly on `PCGPin`.
**Always inspect before connecting** — pin names vary significantly by node type:

```python
def pin_labels(pins):
    return [str(p.get_editor_property('properties').get_editor_property('label')) for p in pins]

node, _ = graph.add_node_of_type(unreal.PCGSurfaceSamplerSettings)
print('Inputs:', pin_labels(node.get_editor_property('input_pins')))
print('Outputs:', pin_labels(node.get_editor_property('output_pins')))
```

**Known pin label gotchas (verified in UE 5.7):**

| Node | Main input pin | Main output pin |
|---|---|---|
| Graph Input node | *(no input)* | `"In"` |
| Graph Output node | `"Out"` | *(no output)* |
| Surface Sampler | `"Surface"` (not `"In"`) | `"Out"` |
| Most other nodes | `"In"` | `"Out"` |

The graph Input/Output node pin naming is inverted from what you'd expect — this is intentional in PCG.

## Checking Pin Connection State

`is_connected()` is a **method**, not a property — call it, don't get_editor_property it:

```python
pin = node.get_editor_property('output_pins')[0]
print(pin.is_connected())          # correct
# print(pin.get_editor_property('is_connected'))  # WRONG — raises AttributeError
```

## Connecting Nodes (Edges)

Use `graph.add_edge()` or `node.add_edge_to()` — both work, prefer `add_edge` for clarity.
Always inspect pin labels first (see above).

```python
import unreal

input_node = graph.get_input_node()
output_node = graph.get_output_node()

# Input node's output pin is labelled "In" — this is correct
graph.add_edge(input_node, 'In', sampler_node, 'Surface')

# Most other nodes use 'Out' → 'In'
graph.add_edge(sampler_node, 'Out', filter_node, 'In')
graph.add_edge(filter_node, 'Out', spawner_node, 'In')

# Output node's input pin is labelled "Out" — this is correct
graph.add_edge(spawner_node, 'Out', output_node, 'Out')
```

`add_edge` returns the destination node, enabling chaining:
```python
graph.add_edge(a, 'Out', b, 'In').add_edge_to('Out', c, 'In')
```

`add_edge` is **permissive and idempotent** — calling it twice on the same pins does not create
duplicate edges (deduplicates silently). It does not error on wrong pin labels — it silently
no-ops if a label doesn't match. Always verify with `pin.is_connected()` after wiring.

## Node Positioning

`get_node_position()` returns a plain `tuple`, not a named object:

```python
node.set_node_position(200, 0)
x, y = node.get_node_position()  # unpack as tuple
```

## Removing Edges and Nodes

```python
import unreal

# Remove a specific edge — returns True if removed, False if edge didn't exist (no crash)
removed = graph.remove_edge(from_node, 'Out', to_node, 'In')

# Remove from the source node side
from_node.remove_edge_to('Out', to_node, 'In')

# Delete a node (also removes its connected edges)
graph.remove_node(node)

# Bulk removal — pass the list explicitly
nodes_to_remove = list(graph.get_editor_property('nodes'))
for n in nodes_to_remove:
    graph.remove_node(n)
```

## Notifying the Editor

`PCGGraph` has no `notify_graph_changed` or `force_notification_for_editor` method — both silently
do nothing or raise `AttributeError`. The only reliable way to refresh the PCG editor after
Python-driven changes is to reload the package:

```python
import unreal

pkg = unreal.find_package('/Game/PCG/MyPCGGraph')
unreal.EditorLoadingAndSavingUtils.reload_packages([pkg])
```

This forces the editor to re-read the asset from disk, so **always save before reloading**:

```python
unreal.EditorAssetLibrary.save_asset('/Game/PCG/MyPCGGraph', only_if_is_dirty=False)
pkg = unreal.find_package('/Game/PCG/MyPCGGraph')
unreal.EditorLoadingAndSavingUtils.reload_packages([pkg])
```

## Saving

**Always save AND reload after every change** — without the reload the PCG graph editor window will not update and users will think nothing happened:

```python
unreal.EditorAssetLibrary.save_asset('/Game/PCGTest/MyGraph', only_if_is_dirty=False)
pkg = unreal.find_package('/Game/PCGTest/MyGraph')
unreal.EditorLoadingAndSavingUtils.reload_packages([pkg])
```

Never call just `save_asset` alone — always follow it with `reload_packages`.

## Deleting PCG Graph Assets

```python
unreal.EditorAssetLibrary.delete_asset('/Game/PCG/MyPCGGraph')
```

**If deletion fails (returns False or raises a permission error), this is a Windows file system
limitation — not a Vibe or PCG API issue.** UE holds an open file handle on every `.uasset` it
loads during a session. That handle is not released until UE shuts down, even after closing the
editor tab or running garbage collection.

To reliably delete PCG assets:

1. Close the editor tab first:
```python
subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
asset = unreal.load_asset('/Game/PCG/MyPCGGraph')
subsystem.close_all_editors_for_asset(asset)
```

2. If graphs reference each other (e.g. a subgraph node), break the cross-reference first,
   save, then delete — otherwise UE refuses deletion even with the tab closed:
```python
# Remove the subgraph node before deleting the subgraph asset
for n in list(main_graph.nodes):
    if type(n.get_settings()).__name__ == 'PCGSubgraphSettings':
        main_graph.remove_node(n)
unreal.EditorAssetLibrary.save_asset('/Game/PCG/MainGraph', only_if_is_dirty=False)
unreal.SystemLibrary.collect_garbage()
unreal.EditorAssetLibrary.delete_asset('/Game/PCG/SubGraph')
```

3. If deletion still fails after steps 1–2, the asset was loaded earlier in the session and UE
   will not release the handle until restart. **Restart UE** — the files will be deletable
   immediately on reboot before the project loads them again.

## Placing a PCGVolume and Generating

**Always check for existing PCGVolume actors before spawning** — if one already exists, use it. Only spawn if none is found.

```python
import unreal, time

# Get actors — use EditorActorSubsystem, NOT EditorWorldSubsystem or Level.get_actors()
actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_subsys.get_all_level_actors()

# Check for existing volume first
existing = [a for a in actors if a.get_class().get_name() == 'PCGVolume']
if existing:
    volume = existing[0]
else:
    volume = actor_subsys.spawn_actor_from_class(
        unreal.PCGVolume, unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0)
    )
    volume.set_actor_scale3d(unreal.Vector(10, 10, 5))

# Actor location uses get_actor_location(), NOT get_location()
print(f'Volume at: {volume.get_actor_location()}')

# Assign graph — graph property is protected, must use set_graph()
graph = unreal.load_asset('/Game/PCGTest/CubeScatter')
pcg_comp = volume.get_component_by_class(unreal.PCGComponent)
pcg_comp.set_graph(graph)

# generate() requires force as a keyword argument
pcg_comp.generate(force=True)

time.sleep(3)

# Verify — ISM components hold the spawned instances
comps = volume.get_components_by_class(unreal.InstancedStaticMeshComponent)
total = sum(c.get_instance_count() for c in comps)
print(f'ISM components: {len(comps)}, total instances: {total}')
```

## Full Example — Surface Sampler → Mesh Spawner

```python
import unreal

# Create asset
factory = unreal.PCGGraphFactory()
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
graph = asset_tools.create_asset('BP_Scatter', '/Game/PCG', unreal.PCGGraph, factory)

input_node = graph.get_input_node()
output_node = graph.get_output_node()
output_node.set_node_position(800, 0)

# Surface Sampler — main input pin is "Surface", not "In"
sampler_node, sampler_settings = graph.add_node_of_type(unreal.PCGSurfaceSamplerSettings)
sampler_settings.set_editor_property('points_per_squared_meter', 2.0)
sampler_node.set_node_position(200, 0)

# Static Mesh Spawner
spawner_node, _ = graph.add_node_of_type(unreal.PCGStaticMeshSpawnerSettings)
spawner_node.set_node_position(500, 0)

# Wire up — note Input node's output pin is "In", Output node's input pin is "Out"
graph.add_edge(input_node, 'In', sampler_node, 'Surface')
graph.add_edge(sampler_node, 'Out', spawner_node, 'In')
graph.add_edge(spawner_node, 'Out', output_node, 'Out')

# Save and reload to refresh the PCG editor
unreal.EditorAssetLibrary.save_asset('/Game/PCG/BP_Scatter', only_if_is_dirty=False)
pkg = unreal.find_package('/Game/PCG/BP_Scatter')
unreal.EditorLoadingAndSavingUtils.reload_packages([pkg])
print("PCG graph created successfully")
```

## Assigning a Subgraph Reference

Use `subgraph_override` (not `subgraph` — that's protected):

```python
sub_graph = unreal.EditorAssetLibrary.load_asset('/Game/PCG/MySubgraph')
sg_node, sg_settings = graph.add_node_of_type(unreal.PCGSubgraphSettings)
sg_settings.set_editor_property('subgraph_override', sub_graph)
```

## Common Settings Property Names

Some properties have non-obvious names — always use `discover_python_class` if unsure.
Confirmed names for commonly used settings:

| Settings Class | Property | Correct Name |
|---|---|---|
| `PCGTransformPointsSettings` | scale | `scale_min`, `scale_max`, `uniform_scale` |
| `PCGTransformPointsSettings` | offset | `offset_min`, `offset_max` |
| `PCGTransformPointsSettings` | rotation | `rotation_min`, `rotation_max` |
| `PCGSubgraphSettings` | subgraph ref | `subgraph_override` (not `subgraph`) |
| `PCGSurfaceSamplerSettings` | unbounded | `unbounded` |
| `PCGSurfaceSamplerSettings` | density | `points_per_squared_meter` |

## Struct Properties Are Lowercase

All UE Python struct types use **lowercase** property names — not CamelCase:

```python
v = unreal.Vector(50, 50, 50)
print(v.x, v.y, v.z)        # correct — NOT v.X, v.Y, v.Z

r = unreal.Rotator(0, 90, 0)
print(r.pitch, r.yaw, r.roll)  # correct — NOT r.Pitch, r.Yaw, r.Roll
```

This applies to `Vector`, `Rotator`, `Transform`, `LinearColor` and all other UE structs in Python.

## Configuring the Static Mesh Spawner

The spawner uses a `mesh_entries` list on its `mesh_selector_parameters`. Each entry has a `descriptor` with a `static_mesh` property. This is the **only** way to assign a mesh — there is no `set_mesh`, `mesh`, or `mesh_selector_type` shortcut:

```python
import unreal

# Get the spawner node's settings
spawner_node, spawner_settings = graph.add_node_of_type(unreal.PCGStaticMeshSpawnerSettings)

# Load a mesh
cube_mesh = unreal.load_asset('/Engine/BasicShapes/Cube')

# Build an entry
entry = unreal.PCGMeshSelectorWeightedEntry()
desc = entry.get_editor_property('descriptor')
desc.set_editor_property('static_mesh', cube_mesh)
entry.set_editor_property('descriptor', desc)
entry.set_editor_property('weight', 1)

# Assign to selector
selector = spawner_settings.get_editor_property('mesh_selector_parameters')
selector.set_editor_property('mesh_entries', [entry])
```

For multiple meshes or materials, pass a list of entries — one per mesh/material combination.

To add colour variation, create one entry per material using `override_materials` on the descriptor. **Do not set the material on `static_mesh`** — that property only accepts a StaticMesh:

```python
import unreal

cube_mesh = unreal.load_asset('/Engine/BasicShapes/Cube')
color_names = ['Red', 'Blue', 'Green', 'Yellow', 'Purple', 'Orange']
materials = [unreal.load_asset(f'/Game/PCGTest/M_Cube_{n}') for n in color_names]

entries = []
for mat in materials:
    entry = unreal.PCGMeshSelectorWeightedEntry()
    desc = entry.get_editor_property('descriptor')
    desc.set_editor_property('static_mesh', cube_mesh)       # mesh goes here
    desc.set_editor_property('override_materials', [mat])    # material goes here
    entry.set_editor_property('descriptor', desc)
    entry.set_editor_property('weight', 1)
    entries.append(entry)

selector = spawner_settings.get_editor_property('mesh_selector_parameters')
selector.set_editor_property('mesh_entries', entries)
```

## Keep Code Blocks Small

PCG operations can be slow if done in bulk. Split large scripts into smaller focused blocks
rather than one monolithic script — avoids 30s execution timeouts and makes errors easier to diagnose.

## Common Node Settings Classes

| Node Type | Settings Class | Main Input Pin |
|---|---|---|
| Surface Sampler | `PCGSurfaceSamplerSettings` | `"Surface"` |
| Mesh Sampler | `PCGMeshSamplerSettings` | `"In"` |
| Static Mesh Spawner | `PCGStaticMeshSpawnerSettings` | `"In"` |
| Density Filter | `PCGDensityFilterSettings` | `"In"` |
| Point Filter | `PCGPointFilterSettings` → runtime: `PCGAttributeFilteringSettings` | `"In"` |
| Transform Points | `PCGTransformPointsSettings` | `"In"` |
| Copy Points | `PCGCopyPointsSettings` | `"Source"`, `"Target"` (no `"In"`) |
| Merge | `PCGMergeSettings` | `"In"` |
| Difference | `PCGDifferenceSettings` | `"In"` |
| Intersection | `PCGIntersectionSettings` | `"In"` |
| Subgraph | `PCGSubgraphSettings` | `"In"` — assign graph via `settings.set_editor_property('subgraph_override', graph_asset)` |
| Create Attribute | `PCGCreateAttributeSettings` | `"In"` |

When in doubt, discover: `[x for x in dir(unreal) if 'PCG' in x and x.endswith('Settings')]`
