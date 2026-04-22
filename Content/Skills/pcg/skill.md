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
for node in graph.get_editor_property('nodes'):
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

`add_edge` is **permissive** — it does not error on duplicate edges or wrong pin labels. It
returns the destination node in both cases. Always verify with `pin.is_connected()` after wiring.

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

Always call after making structural changes so the PCG graph editor refreshes:

```python
graph.force_notification_for_editor(unreal.PCGChangeType.STRUCTURAL)
```

## Saving

```python
unreal.EditorAssetLibrary.save_asset('/Game/PCG/MyPCGGraph')
```

Note: deleting a PCG graph asset while it is open in the PCG editor will fail silently.
Close the asset in the editor first, or use `manage_asset(action="delete", ...)`.

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

# Notify and save
graph.force_notification_for_editor(unreal.PCGChangeType.STRUCTURAL)
unreal.EditorAssetLibrary.save_asset('/Game/PCG/BP_Scatter')
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
