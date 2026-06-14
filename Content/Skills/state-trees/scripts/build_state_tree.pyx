# build_state_tree.pyx — Create a StateTree, add states + a task + a transition, compile.
#
# Sample script for the state-trees skill. Run via execute_python_code.
# State paths are "/"-separated from the subtree name. Create Root with an EMPTY parent_path.
# add_transition(asset, state_path, trigger, transition_type, target_path).
#
# If create_state_tree returns False, the schema isn't registered in this project — the warning
# log lists available schemas. "StateTreeComponentSchema" needs the gameplay StateTree module
# enabled; some projects only expose test/CameraDirector schemas.
import unreal
sts = unreal.StateTreeService

ASSET = "/Game/AI/ST_Guard"
if unreal.EditorAssetLibrary.does_asset_exist(ASSET):
    unreal.EditorAssetLibrary.delete_asset(ASSET)

print("create:", sts.create_state_tree(ASSET, "StateTreeComponentSchema"))

# Build hierarchy: empty parent_path = new top-level subtree
print("Root:", sts.add_state(ASSET, "", "Root"))
print("Patrol:", sts.add_state(ASSET, "Root", "Patrol"))
print("Chase:", sts.add_state(ASSET, "Root", "Chase"))

# Add a task to a state (task struct name; see api-reference.md for available tasks)
print("task:", sts.add_task(ASSET, "Root/Patrol", "FStateTreeDelayTask"))

# Transition Patrol -> Chase on completion
print("transition:", sts.add_transition(ASSET, "Root/Patrol", "OnStateCompleted", "GotoState", "Root/Chase"))

# Compile is REQUIRED after structural changes
r = sts.compile_state_tree(ASSET)
print("compile:", r.success if hasattr(r, "success") else r)
sts.save_state_tree(ASSET)
