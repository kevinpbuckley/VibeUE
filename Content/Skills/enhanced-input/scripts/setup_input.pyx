# setup_input.pyx — Create an Input Action + Mapping Context and bind a key.
#
# Sample script for the enhanced-input skill. Run via execute_python_code.
import unreal
isvc = unreal.InputService

IA = "/Game/Input/IA_Jump"
IMC = "/Game/Input/IMC_Default"

ia = isvc.create_action("IA_Jump", IA, "Digital")        # value_type: Digital(bool)/Axis1D/Axis2D/Axis3D
print("action:", ia)
imc = isvc.create_mapping_context("IMC_Default", IMC, 0)  # priority 0
print("context:", imc)

print("bind SpaceBar:", isvc.add_key_mapping(IMC, IA, "SpaceBar"))
print("mappings:", [str(m) for m in isvc.get_mappings(IMC)])
unreal.EditorAssetLibrary.save_asset(IA)
unreal.EditorAssetLibrary.save_asset(IMC)
