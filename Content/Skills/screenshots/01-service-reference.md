# ScreenshotService Reference

## Overview

`ScreenshotService` captures screenshots of the Unreal Editor window, including Blueprint graphs, Material editors, and any other editor content. This is essential for AI vision capabilities.

## Creating the Service

```python
import unreal
service = unreal.ScreenshotService()
```

## Methods

### capture_editor_window(file_path) → ScreenshotResult

**Recommended method.** Captures the entire Unreal Editor window using Windows DWM.

```python
result = service.capture_editor_window("E:/path/to/screenshot.png")
```

**Works for:**
- Blueprint Event Graphs ✅
- Material Editors ✅
- Widget Blueprint designers ✅
- Level Viewports ✅
- Any editor tab/window ✅

### capture_viewport(file_path, width, height) → ScreenshotResult

Captures only the level viewport. Uses Unreal's built-in screenshot system.

```python
result = service.capture_viewport("E:/path/to/screenshot.png", 1920, 1080)
```

**⚠️ Only works when viewing a level, NOT blueprints!**

### capture_active_window(file_path) → ScreenshotResult

Captures whatever window is currently in focus (may not be UE Editor).

```python
result = service.capture_active_window("E:/path/to/screenshot.png")
```

### get_active_window_title() → str

Returns the title of the currently focused window.

```python
title = service.get_active_window_title()
print(title)  # "FPS57 - Unreal Editor" or "Visual Studio Code", etc.
```

### get_open_editor_tabs() → Array[EditorTabInfo]

Lists all open asset editor tabs.

```python
tabs = service.get_open_editor_tabs()
for tab in tabs:
    print(f"{tab.tab_label} ({tab.tab_type})")
```

### is_editor_window_active() → bool

Checks if the Unreal Editor is the foreground window.

```python
if service.is_editor_window_active():
    print("Editor is in focus")
```

## ScreenshotResult Structure

| Field | Type | Description |
|-------|------|-------------|
| `success` | bool | True if capture succeeded |
| `file_path` | str | Path where file was saved |
| `message` | str | Status message or error |
| `width` | int | Image width in pixels |
| `height` | int | Image height in pixels |
| `captured_window_title` | str | Title of captured window |

## EditorTabInfo Structure

| Field | Type | Description |
|-------|------|-------------|
| `tab_label` | str | Display name of the tab |
| `tab_type` | str | Asset class type |
| `asset_path` | str | Full asset path |
| `is_foreground` | bool | Whether tab is active |
