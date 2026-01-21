# Screenshot Workflows

## Workflow 1: Take Screenshot and Analyze (Recommended)

Use this when user asks to "see", "look at", or "screenshot" anything in the editor.

```python
import unreal

# Step 1: Create screenshot service
service = unreal.ScreenshotService()

# Step 2: Capture the editor window
screenshot_path = "E:/az-dev-ops/FPS57/Saved/Screenshots/Capture.png"
result = service.capture_editor_window(screenshot_path)

# Step 3: Verify success
if result.success:
    print(f"SCREENSHOT_SAVED: {result.file_path}")
    print(f"Size: {result.width}x{result.height}")
    print(f"Window: {result.captured_window_title}")
else:
    print(f"Failed: {result.message}")
```

After executing, call `attach_image` with the screenshot path to analyze it.

## Workflow 2: Screenshot Level Viewport Only

Use when specifically capturing game view (no editor UI).

```python
import unreal

# Method 1: Using ScreenshotService
service = unreal.ScreenshotService()
result = service.capture_viewport("E:/path/screenshot.png", 1920, 1080)

# Method 2: Using AutomationLibrary (legacy)
unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, "E:/path/screenshot.png")
```

**⚠️ Both methods only work when a level viewport is visible!**

## Workflow 3: Check What User Is Viewing

Before taking a screenshot, you can check what's open:

```python
import unreal

service = unreal.ScreenshotService()

# Get open editor tabs
tabs = service.get_open_editor_tabs()
print("Open editors:")
for tab in tabs:
    print(f"  - {tab.tab_label} ({tab.tab_type})")

# Check if editor is in focus
if service.is_editor_window_active():
    print("Editor is focused - safe to capture")
else:
    title = service.get_active_window_title()
    print(f"Another window is focused: {title}")
```

## Workflow 4: Complete Vision Analysis Flow

Full workflow for answering "what am I looking at?":

```python
import unreal
import os

service = unreal.ScreenshotService()

# Capture
screenshot_path = "E:/az-dev-ops/FPS57/Saved/Screenshots/Analysis.png"
result = service.capture_editor_window(screenshot_path)

if result.success and os.path.exists(screenshot_path):
    file_size = os.path.getsize(screenshot_path)
    print(f"SCREENSHOT_SAVED: {screenshot_path}")
    print(f"File size: {file_size:,} bytes")
    
    # Sanity check - very small files may be black screens
    if file_size < 10000:
        print("WARNING: File size very small - may be empty")
else:
    print(f"Capture failed: {result.message}")
```

Then call `attach_image(file_path=screenshot_path)` to include it in your response.
