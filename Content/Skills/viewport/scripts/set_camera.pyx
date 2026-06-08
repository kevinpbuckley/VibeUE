# set_camera.pyx — Position the editor viewport camera and set the view mode (ViewportService).
#
# Sample script for the viewport skill. Run via execute_python_code.
import unreal
vp = unreal.ViewportService

vp.set_camera_location(unreal.Vector(1000, 1000, 800))
vp.set_camera_rotation(unreal.Rotator(-30, -135, 0))
vp.set_view_mode("Lit")        # Lit, Unlit, Wireframe, DetailLighting, etc.
print("viewport:", vp.get_viewport_info())
