# create_system.pyx — Create a Niagara system, add a user parameter, compile.
#
# Sample script for the niagara-systems skill. Run via execute_python_code.
# For emitter internals (modules/renderers/color), load the niagara-emitters skill.
import unreal
ns = unreal.NiagaraService

FOLDER = "/Game/VFX"
NAME = "NS_SkillTest"
asset_path = f"{FOLDER}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
    unreal.EditorAssetLibrary.delete_asset(asset_path)

r = ns.create_system(NAME, FOLDER)   # NiagaraCreateResult
print("create:", r.success, r.asset_path)
system = r.asset_path

# Expose a user parameter (settable per-instance at runtime)
print("add_user_parameter:", ns.add_user_parameter(system, "SpawnRate", "float", "50.0"))
print("params:", [str(p) for p in ns.list_parameters(system)][:10])

ns.compile_system(system, True)
unreal.EditorAssetLibrary.save_asset(system)
