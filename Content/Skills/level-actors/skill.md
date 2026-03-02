---
name: level-actors
display_name: Level Actors & Editor Subsystems
description: Manipulate actors in the current level using editor subsystems
vibeue_classes:
  - ActorService
unreal_classes:
  - EditorActorSubsystem
  - LevelEditorSubsystem
---

# Level Actors Skill

## Critical Rules

### 🚫 DEPRECATED: `unreal.EditorLevelLibrary`

**DO NOT use `unreal.EditorLevelLibrary`.** The entire Editor Scripting Utilities Plugin is deprecated in UE 5.7+. Use `unreal.EditorActorSubsystem` via `unreal.get_editor_subsystem()` for all level actor operations.

### ⚠️ Get Subsystem Instance First

```python
import unreal

actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
```

### ⚠️ Spawning Actors - Use Class Object, NOT Path String

```python
# For built-in classes
actor = subsys.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)

# For Blueprint classes - load first
bp_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/BP_Enemy")
actor = subsys.spawn_actor_from_class(bp_class, location, rotation)
```

### ⚠️ FVector and FRotator Objects Required

```python
# CORRECT
location = unreal.Vector(100, 200, 0)
rotation = unreal.Rotator(0, 90, 0)  # Pitch, Yaw, Roll

# WRONG - strings don't work
location = "(X=100,Y=200,Z=0)"
```

### ⚠️ Refresh Viewport After Changes

```python
level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level_subsys.editor_invalidate_viewports()
```

---

## Workflows

### Spawn Built-in Actor

```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor = subsys.spawn_actor_from_class(
    unreal.StaticMeshActor,
    unreal.Vector(0, 0, 100),
    unreal.Rotator(0, 0, 0)
)

level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level_subsys.editor_invalidate_viewports()
```

### Spawn Blueprint Actor

```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
bp_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Blueprints/BP_Enemy")
if bp_class:
    actor = subsys.spawn_actor_from_class(bp_class, unreal.Vector(500, 0, 100), unreal.Rotator(0, 0, 0))
```

### Get Level Actors

> ⚠️ **`unreal.EditorLevelLibrary` is DEPRECATED.** Always use `EditorActorSubsystem` instead.

```python
import unreal

actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors = actor_subsys.get_all_level_actors()
lights = actor_subsys.get_all_level_actors_of_class(unreal.PointLight)

for actor in all_actors[:10]:
    loc = actor.get_actor_location()
    print(f"{actor.get_name()} at ({loc.x}, {loc.y}, {loc.z})")
```

### Transform Operations

```python
import unreal

# Get/Set location
loc = actor.get_actor_location()
actor.set_actor_location(unreal.Vector(100, 200, 300), False, False)

# Get/Set rotation
rot = actor.get_actor_rotation()
actor.set_actor_rotation(unreal.Rotator(0, 90, 0), False)

# Get/Set scale
scale = actor.get_actor_scale3d()
actor.set_actor_scale3d(unreal.Vector(2, 2, 2))
```

### Selection

```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
subsys.select_nothing()  # Clear
subsys.set_actor_selection_state(actor, True)  # Select
selected = subsys.get_selected_level_actors()  # Get selected
```

### Move Actor to Viewport Camera

```python
import unreal

actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor_subsys = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

camera_info = editor_subsys.get_level_viewport_camera_info()
if camera_info:
    cam_loc, cam_rot = camera_info
    forward_vec = cam_rot.get_forward_vector()
    new_loc = cam_loc + (forward_vec * 200.0)
    actor.set_actor_location(new_loc, False, True)
```

### Destroy/Duplicate Actors

```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Destroy
subsys.destroy_actor(actor)

# Duplicate
dup = subsys.duplicate_actor(actor)
loc = dup.get_actor_location()
dup.set_actor_location(unreal.Vector(loc.x + 200, loc.y, loc.z), False, False)
```

### Camera View Methods (for screenshots and verification)

Use `ActorService` to position the viewport camera to frame actors from specific directions:

```python
import unreal

actor_service = unreal.ActorService

# Move camera to view an actor from above (top-down)
view = actor_service.get_actor_view_camera("MyLandscape", unreal.EViewDirection.TOP)

# Move camera to view from front with extra padding
view = actor_service.get_actor_view_camera("MyBuilding", unreal.EViewDirection.FRONT, 1.5)

# Calculate view without moving camera
view = actor_service.calculate_actor_view("MyActor", unreal.EViewDirection.RIGHT, 1.2)
# view.camera_location, view.camera_rotation, view.view_distance

# Directly set camera to any position/rotation
actor_service.set_viewport_camera(unreal.Vector(1000, 2000, 500), unreal.Rotator(-45, 0, 0))
```

**Directions**: TOP, BOTTOM, LEFT, RIGHT, FRONT, BACK
**Padding**: 1.0=tight, 1.2=default, 2.0=far
