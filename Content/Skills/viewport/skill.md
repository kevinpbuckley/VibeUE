---
name: viewport
display_name: Viewport Control
description: Control the Unreal Editor level viewport - camera type, view mode, FOV, exposure, layout, camera position, and rendering settings
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
| `set_camera_location(vector)` | Set camera world position |
| `set_camera_rotation(rotator)` | Set camera world rotation |
| `set_camera_speed(speed)` | Set camera movement speed (1-8) |
| `set_viewport_layout(name)` | Switch viewport layout (single, quad, etc.) |
| `get_viewport_layout()` | Get current layout name |

---

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

```python
import unreal
# Move camera to a bird's-eye view
unreal.ViewportService.set_camera_location(unreal.Vector(0, 0, 5000))
unreal.ViewportService.set_camera_rotation(unreal.Rotator(-90, 0, 0))
```

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
