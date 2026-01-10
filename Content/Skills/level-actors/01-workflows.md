# Level Actor Workflows

Common patterns for working with actors in the level using editor subsystems.

---

## Spawn Actor in Level

```python
import unreal

# 1. Get editor subsystems
level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Spawn actor at location
location = unreal.Vector(0.0, 0.0, 100.0)
rotation = unreal.Rotator(0.0, 0.0, 0.0)

actor = actor_subsystem.spawn_actor_from_class(
    unreal.StaticMeshActor,
    location,
    rotation
)

if actor:
    print(f"Spawned actor: {actor.get_name()}")

    # 3. Set actor properties
    actor.set_actor_label("MySpawnedActor")
    actor.set_actor_scale3d(unreal.Vector(2.0, 2.0, 2.0))

    # 4. Get static mesh component and set mesh
    static_mesh_comp = actor.static_mesh_component
    if static_mesh_comp:
        mesh = unreal.load_asset("/Game/Meshes/Cube.Cube")
        static_mesh_comp.set_static_mesh(mesh)
```

---

## Spawn Blueprint Actor

```python
import unreal

# 1. Load Blueprint class
bp_class = unreal.load_asset("/Game/Blueprints/BP_Enemy.BP_Enemy_C")

if bp_class:
    # 2. Get subsystem
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    # 3. Spawn actor
    location = unreal.Vector(500.0, 0.0, 100.0)
    rotation = unreal.Rotator(0.0, 90.0, 0.0)

    actor = actor_subsystem.spawn_actor_from_class(
        bp_class,
        location,
        rotation
    )

    if actor:
        print(f"Spawned Blueprint actor: {actor.get_name()}")
        actor.set_actor_label("Enemy_01")
```

---

## Find Actors in Level

```python
import unreal

# 1. Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Get all actors
all_actors = actor_subsystem.get_all_level_actors()
print(f"Total actors in level: {len(all_actors)}")

# 3. Filter by type
static_mesh_actors = []
for actor in all_actors:
    if isinstance(actor, unreal.StaticMeshActor):
        static_mesh_actors.append(actor)

print(f"Static mesh actors: {len(static_mesh_actors)}")

# 4. Filter by name
enemy_actors = []
for actor in all_actors:
    if "Enemy" in actor.get_name():
        enemy_actors.append(actor)

print(f"Enemy actors: {len(enemy_actors)}")
```

---

## Modify Actor Properties

```python
import unreal

# 1. Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Get selected actors
selected_actors = actor_subsystem.get_selected_level_actors()

if len(selected_actors) > 0:
    for actor in selected_actors:
        # Get current location
        location = actor.get_actor_location()
        print(f"{actor.get_name()} at {location}")

        # Move actor up 100 units
        new_location = unreal.Vector(location.x, location.y, location.z + 100.0)
        actor.set_actor_location(new_location, False, True)

        # Rotate actor
        rotation = actor.get_actor_rotation()
        new_rotation = unreal.Rotator(rotation.pitch, rotation.yaw + 45.0, rotation.roll)
        actor.set_actor_rotation(new_rotation, True)

        # Scale actor
        actor.set_actor_scale3d(unreal.Vector(1.5, 1.5, 1.5))

        print(f"Modified: {actor.get_name()}")
else:
    print("No actors selected")
```

---

## Delete Actors

```python
import unreal

# 1. Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Get selected actors
selected_actors = actor_subsystem.get_selected_level_actors()

if len(selected_actors) > 0:
    for actor in selected_actors:
        print(f"Deleting: {actor.get_name()}")
        unreal.EditorLevelLibrary.destroy_actor(actor)

    print(f"Deleted {len(selected_actors)} actors")
else:
    print("No actors selected")
```

---

## Find and Delete Actors by Name Pattern

```python
import unreal

# 1. Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Get all actors
all_actors = actor_subsystem.get_all_level_actors()

# 3. Find actors matching pattern
actors_to_delete = []
for actor in all_actors:
    if "Temp_" in actor.get_name():
        actors_to_delete.append(actor)

# 4. Delete found actors
if len(actors_to_delete) > 0:
    for actor in actors_to_delete:
        print(f"Deleting: {actor.get_name()}")
        unreal.EditorLevelLibrary.destroy_actor(actor)

    print(f"Deleted {len(actors_to_delete)} actors")
else:
    print("No actors found matching pattern")
```

---

## Select Actors

```python
import unreal

# 1. Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Clear selection
actor_subsystem.clear_actor_selection_set()

# 3. Get all actors
all_actors = actor_subsystem.get_all_level_actors()

# 4. Select specific actors
for actor in all_actors:
    if "Enemy" in actor.get_name():
        actor_subsystem.set_actor_selection_state(actor, True)

print("Selected all Enemy actors")
```

---

## Duplicate Selected Actors

```python
import unreal

# 1. Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Get selected actors
selected_actors = actor_subsystem.get_selected_level_actors()

if len(selected_actors) > 0:
    # 3. Duplicate each actor
    duplicated_actors = []

    for actor in selected_actors:
        # Duplicate actor
        duplicated = actor_subsystem.duplicate_actor(actor)

        if duplicated:
            # Offset position
            location = duplicated.get_actor_location()
            new_location = unreal.Vector(location.x + 200.0, location.y, location.z)
            duplicated.set_actor_location(new_location, False, True)

            # Rename
            original_name = actor.get_name()
            duplicated.set_actor_label(f"{original_name}_Copy")

            duplicated_actors.append(duplicated)

    print(f"Duplicated {len(duplicated_actors)} actors")
else:
    print("No actors selected")
```

---

## Spawn Actors in Grid Pattern

```python
import unreal

# 1. Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Grid parameters
grid_size = 5  # 5x5 grid
spacing = 200.0  # 200 units between actors
start_x = -500.0
start_y = -500.0
z = 100.0

# 3. Spawn actors in grid
for x in range(grid_size):
    for y in range(grid_size):
        location = unreal.Vector(
            start_x + (x * spacing),
            start_y + (y * spacing),
            z
        )

        actor = actor_subsystem.spawn_actor_from_class(
            unreal.StaticMeshActor,
            location,
            unreal.Rotator(0.0, 0.0, 0.0)
        )

        if actor:
            actor.set_actor_label(f"GridActor_{x}_{y}")

            # Set mesh
            static_mesh_comp = actor.static_mesh_component
            if static_mesh_comp:
                mesh = unreal.load_asset("/Game/Meshes/Cube.Cube")
                static_mesh_comp.set_static_mesh(mesh)

print(f"Spawned {grid_size * grid_size} actors in grid pattern")
```

---

## Get and Set Actor Tags

```python
import unreal

# 1. Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Get selected actors
selected_actors = actor_subsystem.get_selected_level_actors()

if len(selected_actors) > 0:
    for actor in selected_actors:
        # Get current tags
        tags = actor.tags
        print(f"{actor.get_name()} tags: {tags}")

        # Add new tags
        actor.tags.append("Enemy")
        actor.tags.append("Spawnable")

        print(f"Added tags to {actor.get_name()}")
```

---

## Find Actors by Tag

```python
import unreal

# 1. Get all actors
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors = actor_subsystem.get_all_level_actors()

# 2. Find actors with specific tag
tagged_actors = []
for actor in all_actors:
    if "Enemy" in actor.tags:
        tagged_actors.append(actor)

print(f"Found {len(tagged_actors)} actors with 'Enemy' tag")

# 3. Select found actors
actor_subsystem.clear_actor_selection_set()
for actor in tagged_actors:
    actor_subsystem.set_actor_selection_state(actor, True)
```

---

## Attach Actor to Another Actor

```python
import unreal

# 1. Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Get parent and child actors (assuming selected in order)
selected_actors = actor_subsystem.get_selected_level_actors()

if len(selected_actors) >= 2:
    parent_actor = selected_actors[0]
    child_actor = selected_actors[1]

    # 3. Attach child to parent
    child_actor.attach_to_actor(
        parent_actor,
        "",  # Socket name (empty for root)
        unreal.AttachmentRule.KEEP_WORLD,  # Location rule
        unreal.AttachmentRule.KEEP_WORLD,  # Rotation rule
        unreal.AttachmentRule.KEEP_WORLD,  # Scale rule
        True  # Weld simulated bodies
    )

    print(f"Attached {child_actor.get_name()} to {parent_actor.get_name()}")
else:
    print("Need at least 2 actors selected (parent, then child)")
```

---

## Get Actor Components

```python
import unreal

# 1. Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Get selected actor
selected_actors = actor_subsystem.get_selected_level_actors()

if len(selected_actors) > 0:
    actor = selected_actors[0]

    # 3. Get all components
    components = actor.get_components_by_class(unreal.ActorComponent)

    print(f"\nComponents in {actor.get_name()}:")
    for comp in components:
        print(f"  - {comp.get_name()} ({comp.__class__.__name__})")

        # If it's a static mesh component, get mesh info
        if isinstance(comp, unreal.StaticMeshComponent):
            mesh = comp.static_mesh
            if mesh:
                print(f"    Mesh: {mesh.get_name()}")
```

---

## Modify Component Properties

```python
import unreal

# 1. Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 2. Get selected static mesh actor
selected_actors = actor_subsystem.get_selected_level_actors()

if len(selected_actors) > 0:
    actor = selected_actors[0]

    if isinstance(actor, unreal.StaticMeshActor):
        # Get static mesh component
        mesh_comp = actor.static_mesh_component

        if mesh_comp:
            # Change mesh
            new_mesh = unreal.load_asset("/Game/Meshes/Sphere.Sphere")
            if new_mesh:
                mesh_comp.set_static_mesh(new_mesh)

            # Set material
            material = unreal.load_asset("/Game/Materials/M_Red.M_Red")
            if material:
                mesh_comp.set_material(0, material)

            # Enable/disable collision
            mesh_comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)

            # Set mobility
            mesh_comp.set_mobility(unreal.ComponentMobility.MOVABLE)

            print(f"Modified {actor.get_name()}")
```

---

## Save and Load Level

```python
import unreal

# 1. Get level subsystem
level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# 2. Save current level
current_level = level_subsystem.get_current_level()
if current_level:
    success = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
        save_map_packages=True,
        save_content_packages=False
    )
    if success:
        print("Level saved")

# 3. Load different level
level_to_load = "/Game/Maps/TestLevel"
success = unreal.EditorLoadingAndSavingUtils.load_map(level_to_load)
if success:
    print(f"Loaded level: {level_to_load}")
```

---

## Common Patterns Summary

### Pattern 1: Get Subsystem → Perform Operation
```python
import unreal

# Get subsystem
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Perform operation
actors = actor_subsystem.get_all_level_actors()
```

### Pattern 2: Filter Actors by Type
```python
import unreal

all_actors = actor_subsystem.get_all_level_actors()
filtered = [a for a in all_actors if isinstance(a, unreal.StaticMeshActor)]
```

### Pattern 3: Modify Selected Actors
```python
import unreal

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
selected = actor_subsystem.get_selected_level_actors()

for actor in selected:
    # Modify actor
    actor.set_actor_location(unreal.Vector(0, 0, 100))
```

### Pattern 4: Spawn → Configure → Name
```python
import unreal

# Spawn
actor = actor_subsystem.spawn_actor_from_class(class, location, rotation)

# Configure
actor.set_actor_scale3d(unreal.Vector(2, 2, 2))

# Name
actor.set_actor_label("MyActor")
```

### Pattern 5: Find → Filter → Process
```python
import unreal

# Find all
all_actors = actor_subsystem.get_all_level_actors()

# Filter
enemies = [a for a in all_actors if "Enemy" in a.get_name()]

# Process
for enemy in enemies:
    # Do something
    pass
```
