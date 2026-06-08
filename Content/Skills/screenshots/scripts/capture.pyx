# capture.pyx — Capture the editor window or an asset editor for AI vision analysis.
#
# Sample script for the screenshots skill. Run via execute_python_code.
# After capture, attach the returned file_path with attach_image to let Claude "see" it.
import unreal
ss = unreal.ScreenshotService

out = unreal.Paths.project_saved_dir() + "Screenshots/skill_capture.png"
r = ss.capture_active_window(out)
print("active window:", r)

# Or capture a specific asset's editor (opens+focuses+captures in one call):
# r = ss.capture_asset_editor("/Game/Materials/M_Foo", unreal.Paths.project_saved_dir()+"Screenshots/mat.png")
# print("asset editor:", r.file_path if hasattr(r,'file_path') else r)
