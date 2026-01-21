# Attaching Images for AI Analysis

## The attach_image Tool

After capturing a screenshot, use the `attach_image` tool to include it in your analysis.

```python
attach_image(file_path="E:/az-dev-ops/FPS57/Saved/Screenshots/Capture.png")
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `file_path` | string | Yes | Absolute path to the image file |

### Supported Formats

- PNG ✅ (recommended)
- JPG/JPEG ✅
- BMP ✅
- GIF ✅
- WEBP ✅

## How It Works

1. You execute Python code to capture screenshot
2. You call `attach_image` with the file path
3. The image is included in your **next response**
4. You can then describe what you see

## Complete Example

When user asks: "Take a screenshot and tell me what you see"

**Step 1:** Execute screenshot capture
```python
import unreal

service = unreal.ScreenshotService()
path = "E:/az-dev-ops/FPS57/Saved/Screenshots/Capture.png"
result = service.capture_editor_window(path)
print(f"SCREENSHOT_SAVED: {path}")
```

**Step 2:** Attach the image
```python
attach_image(file_path="E:/az-dev-ops/FPS57/Saved/Screenshots/Capture.png")
```

**Step 3:** The image appears in your context, analyze and describe it.

## Important Notes

⚠️ **Internal Tool Only** - `attach_image` works only in the VibeUE chat window, not via external MCP clients.

⚠️ **File Must Exist** - The screenshot file must exist before calling attach_image.

⚠️ **One Image Per Attach** - Each call attaches one image. For multiple images, call multiple times.

## When to Use Which Screenshot Method

| Situation | Method |
|-----------|--------|
| Viewing a Blueprint | `capture_editor_window()` |
| Viewing a Material | `capture_editor_window()` |
| Viewing a Widget BP | `capture_editor_window()` |
| Viewing a Level | Either method works |
| Game viewport only | `capture_viewport()` |
| Any focused window | `capture_active_window()` |
