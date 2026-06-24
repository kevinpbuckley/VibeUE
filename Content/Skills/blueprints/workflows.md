---
name: blueprints/workflows
description: Step-by-step Blueprint asset workflows — create a Blueprint with variables, add components with properties, add event dispatchers, and implement interfaces.
---

# Blueprint Workflows

## Contents
- Create a Blueprint with variables
- Add components with properties
- Event dispatcher (multicast delegate)
- Implement a Blueprint interface

Each workflow has a matching runnable example under `scripts/`.

## Create a Blueprint with variables

Blueprint creation, variable adds, and compile are engine `BlueprintTools` toolset calls now
(those `BlueprintService` methods were cut). Drive them with `call_tool`:

```python
# 1. Create the Blueprint — BlueprintTools.create (folder_path, asset_name, asset_type)
call_tool(
    tool_name="create",
    toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
    arguments={"folder_path": "/Game/Blueprints", "asset_name": "BP_Player", "asset_type": "Character"},
)
path = "/Game/Blueprints/BP_Player"

# 2. Add variables — BlueprintTools.add_variable (blueprint, name, type_name)
for name, type_name in (("Health", "float"), ("IsAlive", "bool")):
    call_tool(
        tool_name="add_variable",
        toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
        arguments={"blueprint": path, "name": name, "type_name": type_name},
    )

# 3. Compile — BlueprintTools.compile_blueprint (blueprint)
call_tool(
    tool_name="compile_blueprint",
    toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
    arguments={"blueprint": path},
)

import unreal
unreal.EditorAssetLibrary.save_asset(path)
```

Runnable: `scripts/create_blueprint.txt`.

## Add components with properties

`add_component` / `set_component_property` are surviving VibeUE delta methods. Compile is the engine
`BlueprintTools.compile_blueprint` toolset call (a small helper keeps it readable):

```python
import unreal
bs = unreal.BlueprintService
bp = "/Game/Blueprints/BP_Player"

def compile_bp(path):
    return call_tool(
        tool_name="compile_blueprint",
        toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
        arguments={"blueprint": path},
    )

bs.add_component(bp, "StaticMeshComponent", "BodyMesh")
bs.add_component(bp, "PointLightComponent", "Glow", "BodyMesh")  # child
compile_bp(bp)
bs.set_component_property(bp, "Glow", "Intensity", "5000.0")     # values are strings
bs.set_component_property(bp, "Glow", "LightColor", "(R=255,G=127,B=0,A=255)")  # FColor = bytes 0-255, NOT 0-1 floats
compile_bp(bp)
unreal.EditorAssetLibrary.save_asset(bp)
```

Components added without a parent go to root level: the first scene component replaces
DefaultSceneRoot as root; later parentless ones become floating siblings. Pass the parent name,
or use `reparent_component` / `set_root_component`, to build a hierarchy.

Runnable: `scripts/add_component.txt`.

## Event dispatcher (multicast delegate)

```python
import unreal
bs = unreal.BlueprintService
bp = "/Game/Blueprints/BP_Player"

bs.add_event_dispatcher(bp, "OnDied")
bs.add_event_dispatcher_parameter(bp, "OnDied", "Killer", "Actor")
call_id = bs.add_call_delegate_node(bp, "EventGraph", "OnDied", 1400, -700)

# Compile via the engine BlueprintTools toolset (compile_blueprint was cut from BlueprintService):
call_tool(
    tool_name="compile_blueprint",
    toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
    arguments={"blueprint": bp},
)
unreal.EditorAssetLibrary.save_asset(bp)
```

Subscribe elsewhere with `add_delegate_bind_on_variable` (preferred, when the dispatcher lives on a
variable's class) or `add_delegate_bind_node` (lower-level). Runnable: `scripts/event_dispatcher.txt`.

## Implement a Blueprint interface

```python
import unreal
bs = unreal.BlueprintService
bp = "/Game/Blueprints/BP_Player"

bs.add_interface(bp, "/Game/Interfaces/BPI_Interactable")  # prefer full path; idempotent
unreal.EditorAssetLibrary.save_asset(bp)
# Implement interface functions with override_function(bp, "OnInteract")
```

Runnable: `scripts/add_interface.txt`.
