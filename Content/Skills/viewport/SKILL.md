---
name: viewport
display_name: Viewport Control
description: Control the Unreal Editor level viewport — camera type/position, view mode, FOV, exposure, layout, and rendering settings (ViewportService). Use when the user asks to move the editor camera, change the view mode (Lit/Unlit/Wireframe), set FOV/exposure, switch viewport layout, or frame the level for a screenshot.
vibeue_classes:
  - ViewportService
unreal_classes:
  - LevelEditorSubsystem
keywords:
  - viewport
  - camera
  - perspective
  - orthographic
  - top
  - front
  - wireframe
  - fov
  - exposure
  - quad
  - layout
  - game view
  - realtime
---

# Viewport Control Skill

## Methods

| Method | Description |
|--------|-------------|
| `get_viewport_info()` | Get full viewport state (type, camera, FOV, exposure, layout) |
| `set_viewport_type(type)` | Switch perspective/orthographic views |
| `get_viewport_type()` | Get current view type as string |
| `set_view_mode(mode)` | Switch rendering mode (lit, wireframe, unlit, etc.) |
| `get_view_mode()` | Get current rendering mode |
| `set_fov(degrees)` | Set field of view (5-170, default 90) |
| `get_fov()` | Get current FOV |
| `set_near_clip_plane(distance)` | Set near clipping plane (-1 = engine default) |
| `set_far_clip_plane(distance)` | Set far clipping plane (0 = infinity) |
| `set_exposure(fixed, ev100)` | Set fixed/auto exposure with EV100 value |
| `set_exposure_game_settings()` | Reset exposure to auto (game settings) |
| `set_game_view(enable)` | Toggle Game View (hides editor icons) |
| `set_allow_cinematic_control(allow)` | Allow Sequencer to control viewport camera |
| `set_realtime(enable)` | Toggle realtime rendering |
| `set_camera_speed(speed)` | Set camera movement speed (1-8) |
| `set_viewport_layout(name)` | Switch viewport layout (single, quad, etc.) |
| `get_viewport_layout()` | Get current layout name |

---

> 🔀 **Camera get/set moved to the engine.** `ViewportService` no longer has
> `set_camera_location` / `set_camera_rotation`. Reading and positioning the viewport camera is
> now Unreal 5.8's native **`EditorAppToolset`** (`GetCameraTransform` / `SetCameraTransform`, plus
> `FocusOnActors` to frame actors) — reach it with `call_tool` (run `describe_toolset` on
> `EditorAppToolset` for exact action names/params). `ViewportService` still owns view type, view
> mode, FOV, clip planes, exposure, game view, layout, realtime, and camera speed.

## Critical Rules

### ⚠️ Valid View Types for `set_viewport_type`

Only these exact strings are accepted (case-insensitive):

| String | View |
|--------|------|
| `"perspective"` | Perspective (3D) |
| `"top"` | Top-down (XY) |
| `"bottom"` | Bottom-up (-XY) |
| `"left"` | Left side (-XZ) |
| `"right"` | Right side (XZ) |
| `"front"` | Front (-YZ) |
| `"back"` | Back (YZ) |

```python
# ✅ CORRECT
unreal.ViewportService.set_viewport_type("perspective")
unreal.ViewportService.set_viewport_type("top")

# ❌ WRONG — these are not valid strings
unreal.ViewportService.set_viewport_type("ortho")
unreal.ViewportService.set_viewport_type("iso")
```

### ⚠️ Valid View Modes for `set_view_mode`

| String | Rendering Mode |
|--------|----------------|
| `"lit"` | Default fully lit |
| `"unlit"` | No lighting |
| `"wireframe"` | Wireframe overlay |
| `"detaillighting"` | Detail lighting |
| `"lightingonly"` | Lighting only (no textures) |
| `"lightcomplexity"` | Light complexity heatmap |
| `"shadercomplexity"` | Shader complexity heatmap |
| `"pathtracing"` | Path tracing |
| `"clay"` | Clay rendering |

### ⚠️ Valid Layout Names for `set_viewport_layout`

| Name | Layout |
|------|--------|
| `"OnePane"` | Single viewport (default) |
| `"TwoPanesHoriz"` | Two side-by-side |
| `"TwoPanesVert"` | Two stacked |
| `"ThreePanesLeft"` | Large left + 2 right |
| `"ThreePanesRight"` | Large right + 2 left |
| `"ThreePanesTop"` | Large top + 2 bottom |
| `"ThreePanesBottom"` | Large bottom + 2 top |
| `"FourPanesLeft"` | Large left + 3 |
| `"FourPanesRight"` | Large right + 3 |
| `"FourPanesTop"` | Large top + 3 |
| `"FourPanesBottom"` | Large bottom + 3 |
| `"FourPanes2x2"` | Quad view (2x2 grid) |
| `"Quad"` | Alias for FourPanes2x2 |

### 🚨 FOV Only Works in Perspective Mode

Setting FOV has no effect in orthographic views. Always switch to perspective first:

```python
unreal.ViewportService.set_viewport_type("perspective")
unreal.ViewportService.set_fov(75.0)
```

### 🚨 Realtime Mode vs On-Demand

When `set_realtime(False)`, the viewport only repaints on interaction. All ViewportService methods force a redraw after changes, so this is transparent to Python callers — but be aware users won't see continuous animation/particles until realtime is re-enabled.

> ⚠️ **Multi-pane read-back caveat:** `get_viewport_info().is_realtime` reflects the *active* pane.
> In a multi-pane layout (e.g. `FourPanes2x2`) where the active pane isn't the primary, `set_realtime(True)`
> can succeed yet `is_realtime` reads back `False`. Verify realtime in `OnePane` layout, or don't rely on
> the read-back to gate logic when in a split layout. (Other fields like view type / FOV / camera read back
> correctly across layouts.)

---

## Workflows

### Inspect Current Viewport State

```python
import unreal
info = unreal.ViewportService.get_viewport_info()
view_mode = unreal.ViewportService.get_view_mode()
print(f"Type: {info.viewport_type}")
print(f"View Mode: {view_mode}")
print(f"Location: {info.location}")
print(f"Rotation: {info.rotation}")
print(f"FOV: {info.fov}")
print(f"Layout: {info.layout}")
print(f"Realtime: {info.is_realtime}")
print(f"Game View: {info.is_game_view}")
```

### Cycle Through Orthographic Views

```python
import unreal
for view in ["top", "front", "right", "perspective"]:
    unreal.ViewportService.set_viewport_type(view)
```

### Switch to Quad View and Back

```python
import unreal
# Switch to quad (2x2) layout
unreal.ViewportService.set_viewport_layout("Quad")

# Switch back to single pane
unreal.ViewportService.set_viewport_layout("OnePane")
```

### Set Up Architecture Review Camera

```python
import unreal
# Perspective with narrow FOV for minimal distortion
unreal.ViewportService.set_viewport_type("perspective")
unreal.ViewportService.set_fov(60.0)
unreal.ViewportService.set_game_view(True)
unreal.ViewportService.set_exposure(True, 1.0)  # Fixed exposure
unreal.ViewportService.set_realtime(True)
```

### Wireframe Debugging

```python
import unreal
# Switch to wireframe to inspect mesh topology
unreal.ViewportService.set_view_mode("wireframe")

# Return to normal lit view
unreal.ViewportService.set_view_mode("lit")
```

### Position Camera at Specific Location

Camera placement now lives in the engine **`EditorAppToolset`** (`SetCameraTransform`), called via
`call_tool`. From Python you can still drive it through the editor subsystem:

```python
import unreal
# Move camera to a bird's-eye view (location, rotation)
# ⚠️ Rotator positional order is (Roll, Pitch, Yaw) — always use kwargs
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    unreal.Vector(0, 0, 5000), unreal.Rotator(pitch=-90))
```

To frame specific actors instead of guessing coordinates, prefer the engine
`EditorAppToolset.FocusOnActors` action (or load the `level-actors` skill for
`ActorService.get_actor_view_camera`, which auto-computes a framing position from bounds).

### Configure Clipping Planes

```python
import unreal
# Tighten near clip for close-up work
unreal.ViewportService.set_near_clip_plane(1.0)

# Set far clip to avoid rendering distant objects
unreal.ViewportService.set_far_clip_plane(50000.0)

# Reset to defaults
unreal.ViewportService.set_near_clip_plane(-1)  # Engine default
unreal.ViewportService.set_far_clip_plane(0)     # Infinity
```

### Exposure Control

```python
import unreal
# Fix exposure for consistent lighting review
unreal.ViewportService.set_exposure(True, 1.0)

# Adjust to bright/dark scene
unreal.ViewportService.set_exposure(True, -2.0)  # Darker
unreal.ViewportService.set_exposure(True, 4.0)   # Brighter

# Return to auto exposure (game settings)
unreal.ViewportService.set_exposure_game_settings()
```

## Sample scripts (run via `execute_python_code`)

- **`scripts/set_camera.txt`** — position the editor camera and set the view mode.
