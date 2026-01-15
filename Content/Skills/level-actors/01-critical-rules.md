# Level Actors Critical Rules

---

## 📋 Service Discovery

Discover Actor services with module search:

```python
# Use discover_python_module to find Actor services
discover_python_module(module_name="unreal", name_filter="ActorService", include_classes=True)
# Returns: ActorService

# Then discover specific service methods:
discover_python_class(class_name="unreal.ActorService")
```

---

## ⚠️ CRITICAL: Get Subsystem Instance First

Editor subsystems require getting an instance via `get_editor_subsystem()`:

```python
import unreal

# Get subsystem instance
actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# Now call methods
actor_subsys.spawn_actor_from_class(...)
level_subsys.editor_invalidate_viewports()
```

---

## ⚠️ CRITICAL: Spawning Actors

Use `spawn_actor_from_class()` with a class object, NOT a path string:

```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# For built-in classes
actor = subsys.spawn_actor_from_class(
    unreal.StaticMeshActor,  # Class object, NOT string
    unreal.Vector(0, 0, 0),
    unreal.Rotator(0, 0, 0)
)

# For Blueprint classes - load first
bp_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/BP_Enemy")
if bp_class:
    actor = subsys.spawn_actor_from_class(bp_class, location, rotation)
```

---

## ⚠️ CRITICAL: FVector and FRotator

Must use `unreal.Vector` and `unreal.Rotator` objects:

```python
import unreal

# CORRECT
location = unreal.Vector(100, 200, 0)
rotation = unreal.Rotator(0, 90, 0)  # Pitch, Yaw, Roll

# WRONG - strings don't work
location = "(X=100,Y=200,Z=0)"  # This won't work!
```

---

## ⚠️ CRITICAL: Get All Actors

```python
import unreal

# Get all actors of a type
actors = unreal.EditorLevelLibrary.get_all_level_actors()
for actor in actors:
    print(f"{actor.get_name()}: {actor.get_class().get_name()}")

# Filter by class
static_meshes = unreal.EditorLevelLibrary.get_all_level_actors_of_class(unreal.StaticMeshActor)
```

---

## ⚠️ CRITICAL: Selection

```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Select actors
subsys.select_nothing()  # Clear selection
subsys.set_actor_selection_state(actor, True)  # Select single actor

# Get selected actors
selected = subsys.get_selected_level_actors()
```

---

## ⚠️ CRITICAL: Transform Operations

```python
import unreal

# Get location
loc = actor.get_actor_location()
print(f"Location: {loc.x}, {loc.y}, {loc.z}")

# Set location
actor.set_actor_location(unreal.Vector(100, 200, 300), False, False)

# Get/Set rotation
rot = actor.get_actor_rotation()
actor.set_actor_rotation(unreal.Rotator(0, 90, 0), False)

# Get/Set scale
scale = actor.get_actor_scale3d()
actor.set_actor_scale3d(unreal.Vector(2, 2, 2))
```

---

## ⚠️ CRITICAL: Refresh Viewport

After making changes, refresh the viewport:

```python
import unreal

level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level_subsys.editor_invalidate_viewports()
```
