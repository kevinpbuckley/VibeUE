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

## Workflows

### Take Screenshot and Analyze (Recommended)

```python
import unreal

service = unreal.ScreenshotService()
screenshot_path = "E:/Screenshots/Capture.png"
result = service.capture_editor_window(screenshot_path)

if result.success:
    print(f"SCREENSHOT_SAVED: {result.file_path}")
    print(f"Size: {result.width}x{result.height}")
```

After executing, use `attach_image(file_path=screenshot_path)` to analyze.

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
