# Editor Subsystems Reference

Reference for commonly used Unreal Engine editor subsystems when working with level actors.

---

## EditorActorSubsystem

**Access:**
```python
import unreal
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
```

### Key Methods

#### spawn_actor_from_class(actor_class, location, rotation)
Spawn a new actor in the level.

**Returns:** Actor instance or None

**Example:**
```python
import unreal

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

actor = actor_subsystem.spawn_actor_from_class(
    unreal.StaticMeshActor,
    unreal.Vector(0.0, 0.0, 100.0),
    unreal.Rotator(0.0, 0.0, 0.0)
)
```

#### get_all_level_actors()
Get all actors in the current level.

**Returns:** Array of actors

**Example:**
```python
import unreal

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors = actor_subsystem.get_all_level_actors()
print(f"Total actors: {len(all_actors)}")
```

#### get_selected_level_actors()
Get currently selected actors in the editor.

**Returns:** Array of actors

**Example:**
```python
import unreal

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
selected = actor_subsystem.get_selected_level_actors()
print(f"Selected: {len(selected)} actors")
```

#### set_actor_selection_state(actor, should_be_selected)
Set the selection state of an actor.

**Example:**
```python
import unreal

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Select actor
actor_subsystem.set_actor_selection_state(actor, True)

# Deselect actor
actor_subsystem.set_actor_selection_state(actor, False)
```

#### clear_actor_selection_set()
Clear all actor selections.

**Example:**
```python
import unreal

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor_subsystem.clear_actor_selection_set()
```

#### duplicate_actor(actor, to_world=None)
Duplicate an actor.

**Returns:** Duplicated actor or None

**Example:**
```python
import unreal

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

original_actor = actor_subsystem.get_selected_level_actors()[0]
duplicated = actor_subsystem.duplicate_actor(original_actor)

if duplicated:
    # Offset the duplicate
    location = duplicated.get_actor_location()
    duplicated.set_actor_location(
        unreal.Vector(location.x + 200.0, location.y, location.z),
        False,
        True
    )
```

---

## LevelEditorSubsystem

**Access:**
```python
import unreal
level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
```

### Key Methods

#### get_current_level()
Get the currently loaded level.

**Returns:** Level object

**Example:**
```python
import unreal

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
current_level = level_subsystem.get_current_level()
if current_level:
    print(f"Current level: {current_level.get_name()}")
```

#### pilot_level_actor(actor)
Set the editor camera to pilot (control) an actor.

**Example:**
```python
import unreal

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Get first selected actor
selected = actor_subsystem.get_selected_level_actors()
if len(selected) > 0:
    level_subsystem.pilot_level_actor(selected[0])
```

#### eject_pilot_level_actor()
Stop piloting an actor and return camera control to the editor.

**Example:**
```python
import unreal

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level_subsystem.eject_pilot_level_actor()
```

#### editor_set_game_view(game_view)
Toggle between editor view and game view.

**Example:**
```python
import unreal

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# Enable game view
level_subsystem.editor_set_game_view(True)

# Disable game view (back to editor view)
level_subsystem.editor_set_game_view(False)
```

---

## EditorLevelLibrary

Static library for level operations. Does not require subsystem access.

**Access:**
```python
import unreal
# Use directly via unreal.EditorLevelLibrary
```

### Key Methods

#### destroy_actor(actor)
Destroy an actor in the level.

**Returns:** bool

**Example:**
```python
import unreal

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
selected = actor_subsystem.get_selected_level_actors()

for actor in selected:
    unreal.EditorLevelLibrary.destroy_actor(actor)
    print(f"Destroyed: {actor.get_name()}")
```

#### spawn_actor_from_object(object_to_use, location, rotation)
Spawn an actor from an asset object.

**Returns:** Actor instance or None

**Example:**
```python
import unreal

# Load Blueprint asset
bp_asset = unreal.load_asset("/Game/Blueprints/BP_Enemy.BP_Enemy")

# Spawn from Blueprint
actor = unreal.EditorLevelLibrary.spawn_actor_from_object(
    bp_asset,
    unreal.Vector(0.0, 0.0, 100.0),
    unreal.Rotator(0.0, 0.0, 0.0)
)
```

#### get_actor_reference(path_to_actor)
Get an actor by its path in the level.

**Returns:** Actor or None

**Example:**
```python
import unreal

actor = unreal.EditorLevelLibrary.get_actor_reference("PersistentLevel.StaticMeshActor_1")
if actor:
    print(f"Found actor: {actor.get_name()}")
```

#### set_level_viewport_camera_info(camera_location, camera_rotation)
Set the editor viewport camera position and rotation.

**Example:**
```python
import unreal

# Set camera to look at origin from above
camera_location = unreal.Vector(0.0, 0.0, 1000.0)
camera_rotation = unreal.Rotator(-90.0, 0.0, 0.0)

unreal.EditorLevelLibrary.set_level_viewport_camera_info(
    camera_location,
    camera_rotation
)
```

#### get_level_viewport_camera_info()
Get the current editor viewport camera position and rotation.

**Returns:** Tuple of (location, rotation)

**Example:**
```python
import unreal

camera_location, camera_rotation = unreal.EditorLevelLibrary.get_level_viewport_camera_info()
print(f"Camera at: {camera_location}")
print(f"Camera rotation: {camera_rotation}")
```

---

## EditorLoadingAndSavingUtils

Static utilities for loading and saving levels and assets.

**Access:**
```python
import unreal
# Use directly via unreal.EditorLoadingAndSavingUtils
```

### Key Methods

#### save_dirty_packages(save_map_packages, save_content_packages)
Save all unsaved changes.

**Returns:** bool

**Example:**
```python
import unreal

# Save level changes only
success = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
    save_map_packages=True,
    save_content_packages=False
)

if success:
    print("Level saved successfully")

# Save everything (level + content)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
    save_map_packages=True,
    save_content_packages=True
)
```

#### load_map(map_name)
Load a different level/map.

**Returns:** bool

**Example:**
```python
import unreal

# Load specific level
success = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/TestLevel")
if success:
    print("Level loaded")
```

#### new_blank_map(save_existing_map)
Create a new blank level.

**Returns:** bool

**Example:**
```python
import unreal

# Create new level (save current first)
success = unreal.EditorLoadingAndSavingUtils.new_blank_map(save_existing_map=True)
if success:
    print("Created new blank level")
```

---

## Common Actor Methods

These methods are available on all Actor instances.

### Transform

#### get_actor_location()
Get actor world location.

**Returns:** unreal.Vector

**Example:**
```python
location = actor.get_actor_location()
print(f"Actor at: X={location.x}, Y={location.y}, Z={location.z}")
```

#### set_actor_location(new_location, sweep, teleport)
Set actor world location.

**Parameters:**
- `new_location`: unreal.Vector
- `sweep`: bool (check for collisions during move)
- `teleport`: bool (teleport without collision checks)

**Example:**
```python
new_location = unreal.Vector(100.0, 200.0, 50.0)
actor.set_actor_location(new_location, False, True)
```

#### get_actor_rotation()
Get actor rotation.

**Returns:** unreal.Rotator

**Example:**
```python
rotation = actor.get_actor_rotation()
print(f"Rotation: Pitch={rotation.pitch}, Yaw={rotation.yaw}, Roll={rotation.roll}")
```

#### set_actor_rotation(new_rotation, teleport_physics)
Set actor rotation.

**Example:**
```python
new_rotation = unreal.Rotator(0.0, 90.0, 0.0)
actor.set_actor_rotation(new_rotation, True)
```

#### get_actor_scale3d()
Get actor scale.

**Returns:** unreal.Vector

**Example:**
```python
scale = actor.get_actor_scale3d()
print(f"Scale: {scale.x}, {scale.y}, {scale.z}")
```

#### set_actor_scale3d(new_scale)
Set actor scale.

**Example:**
```python
actor.set_actor_scale3d(unreal.Vector(2.0, 2.0, 2.0))
```

### Naming and Organization

#### get_name()
Get actor's internal Unreal name.

**Returns:** str

**Example:**
```python
name = actor.get_name()
print(f"Actor name: {name}")
```

#### get_actor_label()
Get actor's display label in the editor.

**Returns:** str

**Example:**
```python
label = actor.get_actor_label()
print(f"Actor label: {label}")
```

#### set_actor_label(new_label)
Set actor's display label in the editor.

**Example:**
```python
actor.set_actor_label("MyCustomActor")
```

#### set_folder_path(new_folder_path)
Set actor's folder in the Outliner.

**Example:**
```python
actor.set_folder_path("Environment/Props")
```

### Hierarchy

#### attach_to_actor(parent_actor, socket_name, location_rule, rotation_rule, scale_rule, weld_simulated_bodies)
Attach this actor to another actor.

**Parameters:**
- `parent_actor`: Actor to attach to
- `socket_name`: str (socket name, empty for root)
- `location_rule`: unreal.AttachmentRule (KEEP_RELATIVE, KEEP_WORLD, SNAP_TO_TARGET)
- `rotation_rule`: unreal.AttachmentRule
- `scale_rule`: unreal.AttachmentRule
- `weld_simulated_bodies`: bool

**Example:**
```python
child_actor.attach_to_actor(
    parent_actor,
    "",
    unreal.AttachmentRule.KEEP_WORLD,
    unreal.AttachmentRule.KEEP_WORLD,
    unreal.AttachmentRule.KEEP_WORLD,
    True
)
```

#### detach_from_actor(location_rule, rotation_rule, scale_rule)
Detach this actor from its parent.

**Example:**
```python
actor.detach_from_actor(
    unreal.DetachmentRule.KEEP_WORLD,
    unreal.DetachmentRule.KEEP_WORLD,
    unreal.DetachmentRule.KEEP_WORLD
)
```

### Components

#### get_components_by_class(component_class)
Get all components of a specific type.

**Returns:** Array of components

**Example:**
```python
# Get all static mesh components
mesh_components = actor.get_components_by_class(unreal.StaticMeshComponent)
print(f"Found {len(mesh_components)} mesh components")

# Get all audio components
audio_components = actor.get_components_by_class(unreal.AudioComponent)
```

#### get_component_by_class(component_class)
Get first component of a specific type.

**Returns:** Component or None

**Example:**
```python
mesh_comp = actor.get_component_by_class(unreal.StaticMeshComponent)
if mesh_comp:
    print(f"Found mesh component: {mesh_comp.get_name()}")
```

### Tags

#### tags
Actor tags (array of strings).

**Example:**
```python
# Get tags
print(f"Actor tags: {actor.tags}")

# Add tag
actor.tags.append("Enemy")
actor.tags.append("Spawnable")

# Check if has tag
if "Enemy" in actor.tags:
    print("Actor is an enemy")

# Remove tag
if "Spawnable" in actor.tags:
    actor.tags.remove("Spawnable")
```

---

## Complete Example: Spawn and Configure Actors

```python
import unreal

# Get subsystems
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Spawn multiple actors
for i in range(5):
    # Spawn actor
    location = unreal.Vector(i * 200.0, 0.0, 100.0)
    rotation = unreal.Rotator(0.0, i * 45.0, 0.0)

    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor,
        location,
        rotation
    )

    if actor:
        # Set label
        actor.set_actor_label(f"Prop_{i}")

        # Set folder
        actor.set_folder_path("Environment/Props")

        # Add tags
        actor.tags.append("Prop")
        actor.tags.append("Environment")

        # Set scale
        scale = 1.0 + (i * 0.2)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))

        # Configure mesh component
        mesh_comp = actor.static_mesh_component
        if mesh_comp:
            mesh = unreal.load_asset("/Game/Meshes/Cube.Cube")
            mesh_comp.set_static_mesh(mesh)

        print(f"Created and configured: {actor.get_actor_label()}")

print("Complete!")
```
