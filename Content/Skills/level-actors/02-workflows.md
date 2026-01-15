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

---

## Focus Viewport on Actor

```python
import unreal

actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Find and select the actor
all_actors = actor_subsys.get_all_level_actors()
target_actor = next((a for a in all_actors if a.get_actor_label() == "MyActor"), None)

if target_actor:
    # Select the actor
    actor_subsys.set_selected_level_actors([target_actor])
    
    # Focus viewport camera on selection (F key equivalent)
    unreal.SystemLibrary.execute_console_command(None, "Camera Align Selection")
    
    print(f"Focused on {target_actor.get_actor_label()}")
```

---

## Move Actor to Viewport Camera Position

**⚠️ CRITICAL**: Console commands like `"Actor MoveSelectionToCamera"` and `"Actor SnapObjectToView"` are **unreliable**. Use the Python API directly:

```python
import unreal

# Get subsystems
actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor_subsys = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

# Find the actor
all_actors = actor_subsys.get_all_level_actors()
target_actor = next((a for a in all_actors if a.get_actor_label() == "MyActor"), None)

if target_actor:
    # Get viewport camera info
    camera_info = editor_subsys.get_level_viewport_camera_info()
    if camera_info:
        cam_loc, cam_rot = camera_info
        
        # Calculate position in front of camera (200 units)
        forward_vec = cam_rot.get_forward_vector()
        new_loc = cam_loc + (forward_vec * 200.0)
        
        # Move the actor
        target_actor.set_actor_location(new_loc, False, True)
        
        print(f"Moved {target_actor.get_actor_label()} to {new_loc}")
    else:
        print("Could not retrieve viewport camera info")
else:
    print("Actor not found")
```

**Key points:**
- Use `UnrealEditorSubsystem.get_level_viewport_camera_info()` to get camera location and rotation
- Returns tuple: `(camera_location, camera_rotation)` 
- Use `cam_rot.get_forward_vector()` to calculate forward direction
- Multiply forward vector by distance (e.g., 200 units) and add to camera location
- Use `set_actor_location()` to move actor to calculated position
