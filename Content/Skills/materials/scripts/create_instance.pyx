# create_instance.pyx — Create a material instance and override parameters.
#
# Sample script for the materials skill. Run via execute_python_code.
import unreal
ms = unreal.MaterialService

PARENT = "/Game/Materials/M_Character"
NAME = "MI_PlayerRed"
FOLDER = "/Game/Materials/"

r = ms.create_instance(PARENT, NAME, FOLDER)
assert r.success, r.error_message
ip = r.asset_path

ms.set_instance_vector_parameter(ip, "BaseColor", 1.0, 0.0, 0.0, 1.0)
ms.set_instance_scalar_parameter(ip, "Roughness", 0.3)
unreal.EditorAssetLibrary.save_asset(ip)
print("instance:", ip)
