# Level Actor Workflows

---

## Spawn Built-in Actor

```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Spawn StaticMeshActor
actor = subsys.spawn_actor_from_class(
    unreal.StaticMeshActor,
    unreal.Vector(0, 0, 100),
    unreal.Rotator(0, 0, 0)
)
print(f"Spawned: {actor.get_name()}")

# Refresh viewport
level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level_subsys.editor_invalidate_viewports()
```

---

## Spawn Blueprint Actor

```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Load blueprint class
bp_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Blueprints/BP_Enemy")

if bp_class:
    # Spawn at location
    actor = subsys.spawn_actor_from_class(
        bp_class,
        unreal.Vector(500, 0, 100),
        unreal.Rotator(0, 0, 0)
    )
    print(f"Spawned: {actor.get_name()}")
else:
    print("Failed to load blueprint class")
```

---

## Get Level Actors

```python
import unreal

# All actors
all_actors = unreal.EditorLevelLibrary.get_all_level_actors()
print(f"Total actors: {len(all_actors)}")

# Filter by type
lights = unreal.EditorLevelLibrary.get_all_level_actors_of_class(unreal.PointLight)
print(f"Point lights: {len(lights)}")

# List with details
for actor in all_actors[:10]:
    loc = actor.get_actor_location()
    print(f"{actor.get_name()} at ({loc.x}, {loc.y}, {loc.z})")
```

---

## Move Actor

```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Get selected actor
selected = subsys.get_selected_level_actors()
if selected:
    actor = selected[0]
    
    # Move relative
    current = actor.get_actor_location()
    new_loc = unreal.Vector(current.x + 100, current.y, current.z)
    actor.set_actor_location(new_loc, False, False)
    
    # Refresh
    level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level_subsys.editor_invalidate_viewports()
```

---

## Destroy Actor

```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Destroy selected
selected = subsys.get_selected_level_actors()
for actor in selected:
    subsys.destroy_actor(actor)

# Refresh
level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level_subsys.editor_invalidate_viewports()
```

---

## Duplicate Actor

```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Duplicate selected
selected = subsys.get_selected_level_actors()
for actor in selected:
    dup = subsys.duplicate_actor(actor)
    
    # Move duplicate
    loc = dup.get_actor_location()
    dup.set_actor_location(unreal.Vector(loc.x + 200, loc.y, loc.z), False, False)
```
