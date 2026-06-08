# inspect_sequence.pyx — Safely inspect AnimSequences for a skeleton (metadata, no asset loading).
#
# Sample script for the animsequence skill. Run via execute_python_code.
# Use the service discovery methods — NEVER unreal.load_asset() in a loop (memory crash).
import unreal
ass = unreal.AnimSequenceService
sks = unreal.SkeletonService

skeletons = sks.list_skeletons("/Game")
if not skeletons:
    print("no skeletons found");
else:
    skeleton_path = str(skeletons[0]).split(" ")[0]
    anims = ass.find_animations_for_skeleton(skeleton_path)
    print(f"{len(anims)} animations for {skeleton_path}")
    for a in anims[:5]:
        info = ass.get_anim_sequence_info(a.anim_path)
        print(f"  {a.anim_name}: frames={a.frame_count} dur={a.duration:.2f}s")
