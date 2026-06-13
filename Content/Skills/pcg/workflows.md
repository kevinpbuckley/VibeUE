---
name: pcg/workflows
description: PCG graph workflows — inspect/connect pins, position/remove nodes, notify editor, save/delete graphs, place a PCGVolume and generate, full surface-sampler->mesh-spawner example, and subgraph references.
---

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

## Enumerating Edges / verifying the whole graph

There is **no `graph.edges` property** — `graph.get_editor_property('edges')` raises
`AttributeError`. Edges live on each **pin**: `pin.get_editor_property('edges')` returns the list of
`PCGEdge` for that pin. But `PCGEdge` does **not** expose its endpoint pins to Python
(`input_pin`/`output_pin` and every variant are protected), so you cannot traverse an edge to its
other end. To verify wiring, walk the pins instead:

```python
def dump_connections(graph):
    nodes = [graph.get_input_node()] + list(graph.get_editor_property('nodes')) + [graph.get_output_node()]
    for n in nodes:
        name = type(n.get_settings()).__name__ if n.get_settings() else 'Input/Output'
        for pin in n.get_editor_property('output_pins'):
            cnt = len(pin.get_editor_property('edges'))
            if cnt:
                lbl = pin.get_editor_property('properties').get_editor_property('label')
                print(f"{name}.{lbl} -> {cnt} edge(s)")
```

`pin.node` (back-reference to the owning node) and `pin.is_output_pin()` are available; the edge
*count* per pin plus `is_connected()` is the most you can read back. Don't try `PCGEdge.input_pin`.

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

