# multi_bone_edit.pyx — Atomic multi-bone edit (arm chain): preview_pose_delta -> validate -> bake.
#
# Sample script for the animation-editing skill. Run via execute_python_code.
# preview_pose_delta is all-or-nothing: if any bone fails, none are applied.
import unreal
ass = unreal.AnimSequenceService

ANIM = "/Game/Anims/AS_Wave"
FRAME = 15

deltas = [
    unreal.BoneDelta(bone_name="clavicle_r", rotation_delta=unreal.Rotator(0, 0, 15)),
    unreal.BoneDelta(bone_name="upperarm_r", rotation_delta=unreal.Rotator(0, 90, 0)),
    unreal.BoneDelta(bone_name="lowerarm_r", rotation_delta=unreal.Rotator(0, 45, 0)),
    unreal.BoneDelta(bone_name="hand_r",     rotation_delta=unreal.Rotator(0, -30, 20)),
]

r = ass.preview_pose_delta(ANIM, deltas, "local", FRAME)
if r.success and ass.validate_pose(ANIM, True).is_valid:
    ass.bake_preview_to_keyframes(ANIM, 0, -1, "cubic")
    unreal.EditorAssetLibrary.save_asset(ANIM)
    print("applied", len(r.modified_bones), "bone edits")
else:
    ass.cancel_preview(ANIM)
    print("cancelled:", r.error_message)
