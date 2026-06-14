# create_material.pyx — Create a material with BaseColor + Roughness params, compile, verify.
#
# Sample script for the materials skill. Run via execute_python_code.
# Read result.asset_path (MaterialCreateResult) and expr.id (MaterialExpressionInfo) — not the objects.
import unreal
ms, mns = unreal.MaterialService, unreal.MaterialNodeService

NAME = "M_Character"
FOLDER = "/Game/Materials/"
asset_path = FOLDER + NAME
if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
    unreal.EditorAssetLibrary.delete_asset(asset_path)

r = ms.create_material(NAME, FOLDER)
assert r.success, r.error_message
path = r.asset_path

color = mns.create_parameter(path, "Vector", "BaseColor", "Surface", "0.8,0.2,0.2,1.0", -500, 0)
mns.connect_to_output(path, color.id, "", "BaseColor")
rough = mns.create_parameter(path, "Scalar", "Roughness", "Surface", "0.5", -500, 100)
mns.connect_to_output(path, rough.id, "", "Roughness")

ms.compile_material(path)
mns.layout_expressions(path)
unreal.EditorAssetLibrary.save_asset(path)

diag = mns.get_material_diagnostics(path)
print("compiled_ok:", diag.is_compiled_ok, "expressions:", diag.expression_count)
