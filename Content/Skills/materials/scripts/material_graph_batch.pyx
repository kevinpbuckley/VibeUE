# material_graph_batch.pyx — Build a small graph with batch create + connect (one transaction each).
#
# Sample script for the materials skill. Run via execute_python_code.
# Batch ops are much faster than per-node calls. Arrays must be equal length.
import unreal
ms, mns = unreal.MaterialService, unreal.MaterialNodeService

PATH = "/Game/Materials/M_Character"   # must already exist

# Create TintColor (Vector) * Intensity (Scalar) -> BaseColor
tint = mns.create_parameter(PATH, "Vector", "TintColor", "Surface", "1,1,1,1", -600, 0)
inten = mns.create_parameter(PATH, "Scalar", "Intensity", "Surface", "1.0", -600, 150)
mult = mns.create_expression(PATH, "Multiply", -300, 50)

mns.batch_connect_expressions(
    PATH,
    [tint.id, inten.id],     # source ids
    ["", ""],                # source output names ("" = first)
    [mult.id, mult.id],      # target ids
    ["A", "B"],              # target input names
)
mns.connect_to_output(PATH, mult.id, "", "BaseColor")
ms.compile_material(PATH)
unreal.EditorAssetLibrary.save_asset(PATH)
print("diag:", mns.get_material_diagnostics(PATH).is_compiled_ok)
