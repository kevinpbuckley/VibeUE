# capture_preview.pyx — Render an editor-side PNG preview of a Widget Blueprint (no PIE).
#
# Sample script for the umg-widgets skill. Run via execute_python_code.
# capture_preview(widget_path, width=1920, height=1080) takes NO output path — it writes
# to <project>/Saved/WidgetPreviews/<WidgetName>.png and returns the path in result.output_path.
# Use this for appearance checks; use pie_inspect.pyx only when you need runtime state.
import unreal

WIDGET_PATH = "/Game/Blueprints/TestWidget"
WIDTH = 1280
HEIGHT = 720

result = unreal.WidgetService.capture_preview(WIDGET_PATH, WIDTH, HEIGHT)
print("success:", result.success)          # field is `success` (not b_success)
print("output_path:", result.output_path)  # auto-generated under Saved/WidgetPreviews
if not result.success:
    print("error:", result.error_message)
