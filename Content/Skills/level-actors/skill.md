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

### � Creating a "Basic" Level Requires `new_level_from_template`, NOT `new_level`

When the user asks to **create a new level** (especially "Basic", "Default", or with a sky/floor), **always** use `new_level_from_template`:

```python
subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
# ❌ WRONG — creates a completely empty level with no content
subsys.new_level("/Game/Maps/MyLevel")

# ✅ CORRECT — creates level with floor, sky, lighting, player start
subsys.new_level_from_template("/Game/Maps/MyLevel", "/Engine/Maps/Templates/Template_Default")
```

**Template names:** `Template_Default` = Basic, `OpenWorld`, `TimeOfDay_Default`, `VR-Basic`

---

### �🚫 DEPRECATED: `unreal.EditorLevelLibrary`

**DO NOT use `unreal.EditorLevelLibrary`.** The entire Editor Scripting Utilities Plugin is deprecated in UE 5.7+. Use `unreal.EditorActorSubsystem` via `unreal.get_editor_subsystem()` for all level actor operations.

```python
# ❌ DEPRECATED - DO NOT USE
all_actors = unreal.EditorLevelLibrary.get_all_level_actors()
unreal.EditorLevelLibrary.spawn_actor_from_class(...)

# ✅ CORRECT - Use EditorActorSubsystem
actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors = actor_subsys.get_all_level_actors()
actor_subsys.spawn_actor_from_class(...)
```

### 🚫 `get_all_level_actors_of_class` DOES NOT EXIST

**`EditorActorSubsystem` has NO `get_all_level_actors_of_class()` method.** Always use `get_all_level_actors()` + `isinstance()` filtering:

```python
# ❌ WRONG - This method does not exist, causes AttributeError
actor_subsys.get_all_level_actors_of_class(unreal.Landscape)

# ✅ CORRECT - Filter manually
landscapes = [a for a in actor_subsys.get_all_level_actors() if isinstance(a, unreal.Landscape)]
lights = [a for a in actor_subsys.get_all_level_actors() if isinstance(a, unreal.PointLight)]
```

**Migration guide:**
| Deprecated (`EditorLevelLibrary`) | Replacement (`EditorActorSubsystem`) |
|---|---|
| `get_all_level_actors()` | `actor_subsys.get_all_level_actors()` |
| `get_all_level_actors_of_class(cls)` | `[a for a in actor_subsys.get_all_level_actors() if isinstance(a, cls)]` |
| `spawn_actor_from_class()` | `actor_subsys.spawn_actor_from_class()` |
| `destroy_actor()` | `actor_subsys.destroy_actor()` |
| `get_selected_level_actors()` | `actor_subsys.get_selected_level_actors()` |
| `set_actor_selection_state()` | `actor_subsys.set_actor_selection_state()` |

### 🚨 NEVER Use `ActorService.add_actor` for Static Mesh Actors

`ActorService.add_actor()` + `set_property("StaticMesh", ...)` creates actors with **zero-extent bounds** — they are completely invisible in the viewport even though `get_property` reports the mesh is set. This is a known limitation.

**ALWAYS use `spawn_actor_from_class` + `comp.set_static_mesh()`:**

```python
import unreal

actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mesh = unreal.load_asset("/Engine/BasicShapes/Cube")
material = unreal.load_asset("/Game/Materials/M_MyMat")

actor = actor_subsys.spawn_actor_from_class(
    unreal.StaticMeshActor,
    unreal.Vector(0, 0, 100),
    unreal.Rotator(0, 0, 0)
)
actor.set_actor_label("MyMeshActor")
actor.set_actor_scale3d(unreal.Vector(2, 2, 2))

comp = actor.static_mesh_component
comp.set_static_mesh(mesh)
comp.set_material(0, material)   # Optional

# Verify it has real bounds (not 0,0,0)
bounds = actor.get_actor_bounds(False)
print(f"Bounds extent: {bounds[1].x:.0f}, {bounds[1].y:.0f}, {bounds[1].z:.0f}")
```

If bounds extent is `0, 0, 0` after spawning, the mesh was not applied correctly.

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

### Create Level from Template

> 🚨 **Critical:** `new_level()` creates a **blank/empty** level with NO content (no floor, sky, lights, or player start). To get the standard "Basic" level with default content, ALWAYS use `new_level_from_template()` with `/Engine/Maps/Templates/Template_Default`.

**Available UE 5.7 templates in `/Engine/Maps/Templates/`:**
| Template path | Description |
|---|---|
| `/Engine/Maps/Templates/Template_Default` | **Basic** — floor, sky sphere, directional light, player start |
| `/Engine/Maps/Templates/OpenWorld` | Open world with large terrain |
| `/Engine/Maps/Templates/TimeOfDay_Default` | Time-of-day sky setup |
| `/Engine/Maps/Templates/VR-Basic` | VR template |

**Pattern: Create a new level from the Basic template**
```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
save_path  = "/Game/Maps/MyLevel"
template   = "/Engine/Maps/Templates/Template_Default"

# new_level_from_template: closes current level, creates from template, saves + loads
result = subsys.new_level_from_template(save_path, template)
print(f"Created: {result}")  # True on success
```

> ⚠️ **Cannot delete the currently-loaded level.** If the target path already exists and is loaded, you must first switch away using `new_level()` to a temp path, then create from template:
```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
target   = "/Game/Maps/MyLevel"
template = "/Engine/Maps/Templates/Template_Default"

# 1. Switch to temp level to unload target (so it can be overwritten)
subsys.new_level("/Game/Maps/__TempSwitch")

# 2. Create the actual level from template (overwrites any existing asset at target)
result = subsys.new_level_from_template(target, template)
print(f"Created from template: {result}")
# Note: __TempSwitch is automatically replaced/closed — no manual cleanup needed
```

---

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
lights = [a for a in all_actors if isinstance(a, unreal.PointLight)]

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

Use `ActorService` to position the viewport camera to frame actors from specific directions.

> ⚠️ **NEVER guess camera coordinates with `set_viewport_camera`.** Manual positions almost always point at sky or empty space. Always use `get_actor_view_camera` which auto-calculates position from the actor's bounding box.

```python
import unreal

actor_service = unreal.ActorService

# Move camera to view an actor from above (top-down)
view = actor_service.get_actor_view_camera("MyLandscape", unreal.ViewDirection.TOP)

# Move camera to view from front with extra padding
view = actor_service.get_actor_view_camera("MyBuilding", unreal.ViewDirection.FRONT, 1.5)

# Calculate view without moving camera
view = actor_service.calculate_actor_view("MyActor", unreal.ViewDirection.RIGHT, 1.2)
# view.camera_location, view.camera_rotation, view.view_distance
```

**Directions**: TOP, BOTTOM, LEFT, RIGHT, FRONT, BACK
**Padding**: 1.0=tight, 1.2=default, 2.0=far, 3.0=very far (use for large structures)

#### Choosing the Right View

| Goal | Use |
|------|-----|
| Overview of a large structure | `TOP` with padding 2.0–3.0 |
| See the front face | `FRONT` with padding 1.5–2.5 |
| Full castle/building view | `FRONT` padding 3.0, or `TOP` padding 2.5 |
| Check layout from above | `TOP` padding 1.5 |
| Profile view | `LEFT` or `RIGHT` |

> ⚠️ **Do NOT switch to `set_viewport_camera` with manual coordinates to get a "better angle".** Manual positions almost always miss the subject and point at sky or empty space. If the view isn't wide enough, **increase the padding** or switch to `TOP`. There is no need for a diagonal camera — `TOP` and `FRONT` with adequate padding cover every screenshot use case.

---

### Transform Locking & Constraints

#### 🔒 Location Locking (Per-Actor, Native UE5)

UE5 has a native `bLockLocation` property on all actors. When locked, the actor cannot be moved via viewport gizmos (but CAN still be moved via code).

```python
import unreal

# Lock an actor's location
unreal.ActorService.set_actor_lock_location("MyCube", True)

# Check if locked
locked = unreal.ActorService.get_actor_lock_location("MyCube")
print(f"Locked: {locked}")

# Unlock
unreal.ActorService.set_actor_lock_location("MyCube", False)
```

#### 🔒 Scale Ratio Lock (Uniform Scaling Padlock — Global Editor Setting)

The padlock icon next to Scale in the Details panel is the **Preserve Scale Ratio** setting. When enabled, scaling any axis scales ALL axes proportionally. This is a **global editor preference**, not per-actor.

```python
import unreal

# Lock scale axes together (uniform scaling) — the padlock icon
unreal.ActorService.set_preserve_scale_ratio(True)

# Unlock for independent axis scaling
unreal.ActorService.set_preserve_scale_ratio(False)

# Check current state
locked = unreal.ActorService.get_preserve_scale_ratio()
print(f"Scale ratio locked: {locked}")
```

#### ⚠️ There is NO Per-Actor Rotation or Scale Lock

**UE5 does NOT have a per-actor lock for rotation or scale.** There is no `bLockRotation` or `bLockScale` property on actors. Only location locking (`set_actor_lock_location`) is per-actor.

If user asks to "lock rotation" or "lock scale on this actor specifically":
1. Explain this limitation clearly
2. For uniform scaling, use `set_preserve_scale_ratio(True)` (global, affects all actors)
3. For location locking, use `set_actor_lock_location`
4. For world-space independence, use **absolute transform flags**

#### Absolute Transform Flags (Per-Component)

Make location/rotation/scale world-space instead of relative to parent. Useful when attaching actors but needing independent positioning.

```python
import unreal

# Make rotation absolute (independent of parent), keep location/scale relative
unreal.ActorService.set_absolute_transform("MyCube", False, True, False)

# Check flags
loc, rot, scale = unreal.ActorService.get_absolute_transform("MyCube")
print(f"Absolute: loc={loc}, rot={rot}, scale={scale}")
```
