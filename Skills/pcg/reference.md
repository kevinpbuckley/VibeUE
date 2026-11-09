---
name: pcg/reference
description: PCG reference — common settings property names, lowercase struct properties, configuring the Static Mesh Spawner, and common node settings classes.
---

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

r = unreal.Rotator(yaw=90)     # use kwargs — positional order is (Roll, Pitch, Yaw)
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
