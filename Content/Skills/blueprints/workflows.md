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

```python
import unreal
bs = unreal.BlueprintService

path = bs.create_blueprint("Player", "Character", "/Game/Blueprints")  # (name, parent, folder)
bs.add_variable(path, "Health", "float", "100.0")
bs.add_variable(path, "IsAlive", "bool", "true")
bs.compile_blueprint(path)
unreal.EditorAssetLibrary.save_asset(path)
```

Runnable: `scripts/create_blueprint.pyx`.

## Add components with properties

```python
import unreal
bs = unreal.BlueprintService
bp = "/Game/Blueprints/BP_Player"

bs.add_component(bp, "StaticMeshComponent", "BodyMesh")
bs.add_component(bp, "PointLightComponent", "Glow", "BodyMesh")  # child
bs.compile_blueprint(bp)
bs.set_component_property(bp, "Glow", "Intensity", "5000.0")     # values are strings
bs.set_component_property(bp, "Glow", "LightColor", "(R=255,G=127,B=0,A=255)")  # FColor = bytes 0-255, NOT 0-1 floats
bs.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset(bp)
```

Components added without a parent go to root level: the first scene component replaces
DefaultSceneRoot as root; later parentless ones become floating siblings. Pass the parent name,
or use `reparent_component` / `set_root_component`, to build a hierarchy.

Runnable: `scripts/add_component.pyx`.

## Event dispatcher (multicast delegate)

```python
import unreal
bs = unreal.BlueprintService
bp = "/Game/Blueprints/BP_Player"

bs.add_event_dispatcher(bp, "OnDied")
bs.add_event_dispatcher_parameter(bp, "OnDied", "Killer", "Actor")
call_id = bs.add_call_delegate_node(bp, "EventGraph", "OnDied", 1400, -700)
bs.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset(bp)
```

Subscribe elsewhere with `add_delegate_bind_on_variable` (preferred, when the dispatcher lives on a
variable's class) or `add_delegate_bind_node` (lower-level). Runnable: `scripts/event_dispatcher.pyx`.

## Implement a Blueprint interface

```python
import unreal
bs = unreal.BlueprintService
bp = "/Game/Blueprints/BP_Player"

bs.add_interface(bp, "/Game/Interfaces/BPI_Interactable")  # prefer full path; idempotent
unreal.EditorAssetLibrary.save_asset(bp)
# Implement interface functions with override_function(bp, "OnInteract")
```

Runnable: `scripts/add_interface.pyx`.
