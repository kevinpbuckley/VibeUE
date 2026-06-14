# create_blueprint.pyx — Create a Blueprint with variables, then compile + save.
#
# Sample script for the blueprints skill. Run via execute_python_code.
# create_blueprint(name, parent_class, folder) — three separate args (NOT a full path first).
import unreal
bs = unreal.BlueprintService

NAME = "BP_Player"
PARENT_CLASS = "Character"      # e.g. Actor, Pawn, Character, ActorComponent
FOLDER = "/Game/Blueprints"

asset_path = f"{FOLDER}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
    unreal.EditorAssetLibrary.delete_asset(asset_path)

path = bs.create_blueprint(NAME, PARENT_CLASS, FOLDER)   # returns full asset path
print("created:", path)

bs.add_variable(path, "Health", "float", "100.0")
bs.add_variable(path, "IsAlive", "bool", "true")
bs.add_variable(path, "Waypoints", "vector", "", True)   # is_array=True

c = bs.compile_blueprint(path)
print("compile:", c.success, "errors:", c.num_errors)
unreal.EditorAssetLibrary.save_asset(path)
