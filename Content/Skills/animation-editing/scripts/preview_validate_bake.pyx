# preview_validate_bake.pyx — Safe single-bone edit: profile -> preview -> validate -> bake.
#
# Sample script for the animation-editing skill. Run via execute_python_code.
# Always specify bone space ("local" for user intent). Cancel if validation fails.
import unreal
ass = unreal.AnimSequenceService
sks = unreal.SkeletonService

ANIM = "/Game/Anims/AS_Idle"
BONE = "upperarm_r"
DELTA = unreal.Rotator(0, 45, 0)   # Euler (Roll, Pitch, Yaw); raise arm
SPACE = "local"
FRAME = 0

skeleton = ass.get_animation_skeleton(ANIM)
sks.create_skeleton_profile(skeleton)   # constraints come from the profile

r = ass.preview_bone_rotation(ANIM, BONE, DELTA, SPACE, FRAME)
print("preview:", r.success, "clamped:", r.was_clamped, "bones:", list(r.modified_bones))

if r.success:
    v = ass.validate_pose(ANIM, True)   # True = use learned constraints
    if v.is_valid:
        b = ass.bake_preview_to_keyframes(ANIM, 0, -1, "cubic")
        print("baked frames:", b.start_frame, "-", b.end_frame)
        unreal.EditorAssetLibrary.save_asset(ANIM)
    else:
        for m in v.violation_messages:
            print("violation:", m)
        ass.cancel_preview(ANIM)
