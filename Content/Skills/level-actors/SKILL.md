---
name: level-actors
display_name: Level Actors & Editor Subsystems
description: List, find, place, move, rotate, and modify actors in the current level (ActorService + editor subsystems). Use when the user asks to find/list actors in the level, move or rotate an actor, set an actor's properties/folder/transform, or spawn/delete level actors.
vibeue_classes:
  - ActorService
unreal_classes:
  - EditorActorSubsystem
  - LevelEditorSubsystem
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "actors-and-components"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Level Actors Skill

> 🔀 **Engine owns the basics now.** In the Unreal 5.8 consolidation, listing / finding / spawning /
> moving / transforming / selecting actors moved to Unreal's native toolsets — `ActorTools`,
> `SceneTools`, and `EditorAppToolset` (selection, camera, PIE, screenshots), reachable via
> `call_tool`. Equivalently you can drive `unreal.EditorActorSubsystem` /
> `unreal.LevelEditorSubsystem` directly from `execute_python_code` (shown throughout below).
> **VibeUE's `ActorService` was trimmed to only the delta the engine doesn't provide:** transform
> lock (`set_actor_lock_location`), absolute-transform flags, preserve-scale-ratio, viewport camera
> framing (`get_actor_view_camera` / `calculate_actor_view` / `set_viewport_camera`), and full
> property dumps (`get_all_properties`). `ActorService.add_actor` / `set_property` / `get_property`
> no longer exist — use the engine toolsets or `EditorActorSubsystem` for spawning and property edits.

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

### 🚨 Verified `AttributeError` traps (UE 5.7 Python) — don't burn iterations rediscovering these

| You might write | ❌ Why it fails | ✅ Use instead |
|---|---|---|
| `comp.get_static_mesh()` | `StaticMeshComponent` has no `get_static_mesh` | `comp.get_editor_property("static_mesh")` (read) / `comp.set_static_mesh(mesh)` (write) |
| `actor.get_components()` | Actors have no `get_components()` | `actor.get_components_by_class(unreal.StaticMeshComponent)` → array |
| `dir_light.directional_light_component` | no such attribute | `dir_light.light_component` (ALL `unreal.Light` actors use `light_component`) |
| `comp.set_editor_property("cast_shadow", …)` | property is plural | `comp.set_editor_property("cast_shadows", False)` |
| `unreal.HorizontalTextAligment` | UE misspells it | `unreal.HorizTextAligment` / `unreal.VerticalTextAligment` (for `TextRenderActor`) |

When unsure of a property/method name, call `discover_python_class(class_name="unreal.X")` ONCE up front
rather than guessing repeatedly — its `doc_string` lists the real editor-property names.

**Migration guide:**
| Deprecated (`EditorLevelLibrary`) | Replacement (`EditorActorSubsystem`) |
|---|---|
| `get_all_level_actors()` | `actor_subsys.get_all_level_actors()` |
| `get_all_level_actors_of_class(cls)` | `[a for a in actor_subsys.get_all_level_actors() if isinstance(a, cls)]` |
| `spawn_actor_from_class()` | `actor_subsys.spawn_actor_from_class()` |
| `destroy_actor()` | `actor_subsys.destroy_actor()` |
| `get_selected_level_actors()` | `actor_subsys.get_selected_level_actors()` |
| `set_actor_selection_state()` | `actor_subsys.set_actor_selection_state()` |

### 🚨 Spawn Static Mesh Actors via `spawn_actor_from_class` + `comp.set_static_mesh()`

Do **not** spawn a bare actor and then poke `StaticMesh` through a generic property setter — that
historically produced actors with **zero-extent bounds** (invisible in the viewport even though the
mesh reads back as set). (The old `ActorService.add_actor` / `set_property` helpers that caused this
were removed in 5.8.)

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
rotation = unreal.Rotator(pitch=-45)  # ALWAYS use keyword args for Rotator (see rule below)

# WRONG - strings don't work
location = "(X=100,Y=200,Z=0)"
```

### 🚨 Constructor Argument Order Traps: `Rotator` is (Roll, Pitch, Yaw), `Color` is BGRA

The Python constructors do NOT use the order you expect. **Always use keyword arguments** — never positional — for `unreal.Rotator` and `unreal.Color`:

```python
# ❌ WRONG — Rotator positional order is (Roll, Pitch, Yaw), NOT (Pitch, Yaw, Roll)
unreal.Rotator(-45, 0, 0)        # sets ROLL=-45, not pitch!

# ✅ CORRECT — keyword args, unambiguous
unreal.Rotator(pitch=-45)                    # angle down 45°
unreal.Rotator(roll=0, pitch=-90, yaw=45)    # straight down, spun 45°

# ❌ WRONG — Color positional order is (B, G, R, A), NOT (R, G, B, A)
unreal.Color(255, 180, 100)      # comes out BLUE, not warm orange!

# ✅ CORRECT — keyword args, or use LinearColor (which IS r, g, b, a positional)
unreal.Color(r=255, g=180, b=100, a=255)
unreal.LinearColor(1.0, 0.7, 0.4, 1.0)       # LinearColor positional is normal RGBA
```

**Pitch sign convention:** negative pitch = downward (`pitch=-90` points straight down). For lights/cameras use negative pitch to aim at the ground.

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

### Save the Current Level (Save / Save As)

**Getting the editor world** — it lives on `UnrealEditorSubsystem`, NOT `LevelEditorSubsystem`, NOT the deprecated `EditorLevelLibrary`:

```python
import unreal

# ❌ WRONG — AttributeError: 'LevelEditorSubsystem' object has no attribute 'get_editor_world'
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_editor_world()

# ❌ DEPRECATED — Editor Scripting Utilities plugin
unreal.EditorLevelLibrary.get_editor_world()

# ✅ CORRECT
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
```

**Which save call to use:**
- Level already saved at a path → `unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()`
- Untitled/never-saved level, or "save as <new name>" → `EditorLoadingAndSavingUtils.save_map(world, asset_path)` (**both args required**, in that order):

```python
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ok = unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Maps/MyLevel")
print(f"Saved: {ok}")
```

To also make the saved level the editor startup map, load the `project-settings` skill and set `maps` / `EditorStartupMap` with the full asset path (`/Game/Maps/MyLevel.MyLevel`).

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

# Get/Set location — args: (new_location, sweep, teleport)
loc = actor.get_actor_location()
actor.set_actor_location(unreal.Vector(100, 200, 300), False, False)

# Get/Set rotation — ALWAYS keyword args for Rotator (positional is Roll, Pitch, Yaw!)
rot = actor.get_actor_rotation()   # rot.pitch / rot.yaw / rot.roll
actor.set_actor_rotation(unreal.Rotator(pitch=-90, yaw=45), False)

# Relative rotation in LOCAL space (returns HitResult — see sweep section)
actor.add_actor_local_rotation(unreal.Rotator(pitch=30), False, False)
# World-space equivalent: actor.add_actor_world_rotation(...)

# Get/Set scale
scale = actor.get_actor_scale3d()
actor.set_actor_scale3d(unreal.Vector(2, 2, 2))

# Everything at once
actor.set_actor_transform(
    unreal.Transform(
        location=unreal.Vector(500, 500, 200),
        rotation=unreal.Rotator(pitch=45),
        scale=unreal.Vector(1.5, 1.5, 1.5)),
    False, False)
```

### Sweep Moves & Collision Checks (HitResult)

Three verified gotchas (UE 5.7):

1. **`HitResult` fields cannot be read as attributes** — `hit.blocking_hit` / `hit.bBlockingHit` raise "property is protected". Use `hit.to_tuple()[0]` (the blocking-hit bool) or `hit.export_text()` (full readable dump: Location, ImpactPoint, Normal, Distance, Time).
2. **`set_actor_location(loc, sweep=True, ...)` does NOT detect collisions in the editor world** — it reports no hit even when moving straight through another actor. Don't rely on it for collision checks.
3. **The definitive checks:** compare the actor's final position to the target, and/or trace the path explicitly with `SystemLibrary.line_trace_single` (this DOES hit editor actors):

```python
import unreal

actor.set_actor_location(target, True, False)
# ⚠️ Vector.equals(b) takes NO tolerance arg — use distance() for tolerant compare
arrived = unreal.Vector.distance(actor.get_actor_location(), target) < 1.0

# Explicit path check that actually works in the editor:
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
hit = unreal.SystemLibrary.line_trace_single(
    world, start_loc, target,
    unreal.TraceTypeQuery.ECC_VISIBILITY,      # Visibility channel (TRACE_TYPE_QUERY1 is the deprecated alias)
    False, [actor],                            # ignore the moving actor itself
    unreal.DrawDebugTrace.NONE, True)
if hit and hit.to_tuple()[0]:                  # [0] = blocking hit bool
    blocker = hit.to_tuple()[9]                # [9] = hit Actor, [10] = hit Component
    print(f"Blocked by {blocker.get_actor_label()}: {hit.export_text()[:200]}")
```

### Organize: Rename, Folders, Attach/Detach, Tags

```python
import unreal

# Rename (the label shown in the outliner)
actor.set_actor_label("MainLight")

# Outliner folder (creates the folder if needed; "" = root, "A/B" nests)
actor.set_folder_path("TestLights")

# Attach to another actor, keeping current world position
actor.attach_to_actor(parent_actor, "",
    unreal.AttachmentRule.KEEP_WORLD,
    unreal.AttachmentRule.KEEP_WORLD,
    unreal.AttachmentRule.KEEP_WORLD, False)

# Detach, keeping world position
actor.detach_from_actor(
    unreal.DetachmentRule.KEEP_WORLD,
    unreal.DetachmentRule.KEEP_WORLD,
    unreal.DetachmentRule.KEEP_WORLD)

# Actor tags (read/write the whole list; query with `in`)
actor.tags = ["Test", "Lighting"]
tagged = [a for a in actor_subsys.get_all_level_actors() if "Lighting" in a.tags]
```

### Light Actors (PointLight, SpotLight, DirectionalLight)

All `unreal.Light` actors expose their component as `actor.light_component` — no `get_component_by_class` needed:

```python
import unreal

comp = light_actor.light_component        # PointLightComponent / SpotLightComponent / ...
comp.set_intensity(10000)
comp.set_light_color(unreal.LinearColor(1.0, 0.7, 0.4))   # warm; LinearColor IS positional RGBA
intensity = comp.get_editor_property("intensity")
# Spot cone angles (SpotLight only):
# comp.set_inner_cone_angle(20); comp.set_outer_cone_angle(35)
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

Use `ActorService` to position the viewport camera to frame actors from specific directions. (For a
plain "focus on these actors" with no specific direction, the engine **`EditorAppToolset`**
`FocusOnActors` action via `call_tool` also works — `ActorService` adds the directional/padding
framing the engine toolset lacks.)

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

## Sample scripts (run via `execute_python_code`)

- **`scripts/manipulate_actors.txt`** — list level actors, find by class, move/rotate by name.
