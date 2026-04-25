---
name: pcg
display_name: Procedural Content Generation (PCG)
description: Create and edit PCG graphs for procedural world building - add nodes, connect edges, configure settings, and manage PCG components on actors
unreal_classes:
  - PCGComponent
  - PCGGraph
  - PCGEditorGraph
  - PCGNode
keywords:
  - pcg
  - procedural
  - content generation
  - graph
  - nodes
  - spline
  - scatter
  - placement
  - world building
---

# Procedural Content Generation (PCG) Skill

## Overview

PCG (Procedural Content Generation) is a native Unreal Engine framework for procedurally placing and generating content in levels. It uses a graph-based workflow to define rules for spawning meshes, actors, and data points.

## Key Concepts

- **PCGGraph** - Asset containing the node graph that defines procedural rules
- **PCGComponent** - Actor component that executes a PCGGraph and outputs content in the level
- **Nodes** - Graph nodes such as Surface Sampler, Mesh Spawner, Filter, Transform, etc.
- **Edges** - Connections between node pins that pass point data through the graph

## Workflows

### Discover PCG Classes

```python
import unreal
# Discover available PCG-related classes
result = unreal.EditorAssetLibrary.list_assets("/Script/PCG", recursive=True)
print(result)
```

### Create a PCG Graph Asset

```python
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.PCGGraphFactory()
graph = asset_tools.create_asset("PCG_MyGraph", "/Game/PCG", unreal.PCGGraph, factory)
unreal.EditorAssetLibrary.save_asset(graph.get_path_name())
print(f"Created PCG Graph: {graph.get_path_name()}")
```

### Add a PCGComponent to an Actor

```python
import unreal

# Find or spawn an actor in the level
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.Actor, unreal.Vector(0, 0, 0))
component = actor.add_component_by_class(unreal.PCGComponent, False, unreal.Transform(), False)
print(f"Added PCGComponent to {actor.get_name()}")
```

### Assign a Graph to a PCGComponent

```python
import unreal

# Load the PCG graph asset
graph = unreal.load_asset("/Game/PCG/PCG_MyGraph")

# Find actor with PCGComponent
actors = unreal.EditorLevelLibrary.get_all_level_actors()
for actor in actors:
    comp = actor.get_component_by_class(unreal.PCGComponent)
    if comp:
        comp.set_editor_property("graph", graph)
        print(f"Assigned graph to {actor.get_name()}")
        break
```

### Regenerate PCG Output

```python
import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
for actor in actors:
    comp = actor.get_component_by_class(unreal.PCGComponent)
    if comp:
        comp.generate(True)  # True = force regenerate
        print(f"Regenerated PCG on {actor.get_name()}")
```

## Critical Rules

### ⚠️ PCG Plugin Must Be Enabled

The PCG plugin must be enabled in the project before using PCG classes:
- Go to **Edit > Plugins** and search for "PCG" or "Procedural Content Generation"
- Enable the plugin and restart the editor

### ⚠️ Always Discover Exact API Methods

PCG Python bindings vary by UE version. Always discover actual methods:

```python
import unreal
# Discover PCGComponent methods
result = unreal.EditorPythonBridgeLibrary.call_method_on_object(...)
# Or use MCP: discover_python_class("unreal.PCGComponent")
```

### ⚠️ Generate After Graph Changes

After modifying a PCGGraph programmatically, call `generate()` on all PCGComponents using that graph to refresh the output in the level.
