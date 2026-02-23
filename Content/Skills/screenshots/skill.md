---
name: screenshots
display_name: Screenshot & Vision
description: Capture screenshots of the editor window, viewports, and blueprints for AI vision analysis
vibeue_classes:
  - ScreenshotService
unreal_classes:
  - AutomationLibrary
keywords:
  - screenshot
  - capture
  - image
  - vision
---

# Screenshot & Vision Skill

## Methods

| Method | Use Case |
|--------|----------|
| `capture_editor_window(path)` | Blueprint, Material, Widget BP, any editor tab |
| `capture_viewport(path, w, h)` | Level viewport only (not blueprints!) |
| `capture_active_window(path)` | Whatever window is focused |
| `get_active_window_title()` | Check what's in focus |
| `get_open_editor_tabs()` | List open asset editors |

---

## Camera Best Practices for Screenshots

**Before taking a viewport screenshot, adjust the editor camera to get the best view of the subject.** This is critical for useful visual analysis.

### When to Adjust Camera
- **Always** before a viewport screenshot if you know what you're capturing
- When verifying placement, scale, or visual appearance of actors/landscapes/effects
- When the user asks to "show me" or "check" something — frame it well
- When capturing issues — zoom in close enough to see the problem clearly

### How to Adjust Camera

Use `UnrealEditorSubsystem` to get/set the viewport camera:

```python
import unreal

editor_subsys = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

# Get current camera
camera_info = editor_subsys.get_level_viewport_camera_info()
if camera_info:
    cam_loc, cam_rot = camera_info

# Set camera to a new position/rotation
new_location = unreal.Vector(x, y, z)
new_rotation = unreal.Rotator(pitch, yaw, roll)
editor_subsys.set_level_viewport_camera_info(new_location, new_rotation)
```

### Framing Guidelines
- **Landscape/terrain**: Position camera above and angled down (pitch ~ -30 to -45) for a good overview
- **Single actor**: Move camera to ~200-500 units away, facing the actor
- **Small details/issues**: Zoom in close (100-200 units) to clearly show the problem
- **Comparisons**: Frame both subjects in view, or take separate close-ups of each
- **UI/Widgets**: Use `capture_editor_window` instead (no camera adjustment needed)

### Focus on Actor Helper

```python
import unreal

def frame_actor_for_screenshot(actor, distance=500.0, pitch=-25.0):
    """Position camera to look at an actor from a good angle."""
    editor_subsys = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    
    # Get actor bounds for center point
    origin, extent = actor.get_actor_bounds(False)
    
    # Position camera behind and above, looking at the actor
    yaw = 0.0  # Face default direction; adjust as needed
    rot = unreal.Rotator(pitch, yaw, 0.0)
    forward = rot.get_forward_vector()
    cam_loc = origin - (forward * distance)
    
    editor_subsys.set_level_viewport_camera_info(cam_loc, rot)
```

---

## Workflows

### Take Screenshot and Analyze (Recommended)

```python
import unreal

# Step 1: Adjust camera if capturing viewport (optional but recommended)
editor_subsys = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor_subsys.set_level_viewport_camera_info(
    unreal.Vector(0, 0, 2000),       # position above scene
    unreal.Rotator(-45.0, 0.0, 0.0)  # look down at 45 degrees
)

# Step 2: Capture
service = unreal.ScreenshotService()
screenshot_path = "E:/Screenshots/Capture.png"
result = service.capture_viewport(screenshot_path, 1920, 1080)

if result.success:
    print(f"SCREENSHOT_SAVED: {result.file_path}")
    print(f"Size: {result.width}x{result.height}")
```

After executing, use `attach_image(file_path=screenshot_path)` to analyze.

### Take Editor Window Screenshot (No Camera Needed)

```python
import unreal

service = unreal.ScreenshotService()
screenshot_path = "E:/Screenshots/EditorCapture.png"
result = service.capture_editor_window(screenshot_path)

if result.success:
    print(f"SCREENSHOT_SAVED: {result.file_path}")
    print(f"Size: {result.width}x{result.height}")
```

### Check What User Is Viewing

```python
import unreal

service = unreal.ScreenshotService()

tabs = service.get_open_editor_tabs()
for tab in tabs:
    print(f"{tab.tab_label} ({tab.tab_type})")

if service.is_editor_window_active():
    print("Editor is focused")
else:
    print(f"Focused: {service.get_active_window_title()}")
```

### Viewport Only Screenshot

```python
import unreal

service = unreal.ScreenshotService()
result = service.capture_viewport("E:/Screenshots/viewport.png", 1920, 1080)
```

⚠️ Only works when a level viewport is visible!

---

## Data Structures

### ScreenshotResult
- `success` (bool)
- `file_path` (str)
- `message` (str) - Error message if failed
- `width`, `height` (int)
- `captured_window_title` (str)

### EditorTabInfo
- `tab_label` (str) - Display name
- `tab_type` (str) - Asset class type
- `asset_path` (str)
- `is_foreground` (bool)

---

## Attaching Images

After capturing, use `attach_image` tool:

```python
attach_image(file_path="E:/Screenshots/Capture.png")
```

- Supported formats: PNG, JPG, BMP, GIF, WEBP
- File must exist before attaching
- Image appears in your next response for analysis
