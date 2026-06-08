---
name: animation-editing/common-mistakes
description: Animation editing pitfalls — always specify bone space, preview/validate before baking, analyze the reference pose before guessing axes, build a skeleton profile first, and never load assets in loops (crashes); use VibeUE safe-discovery methods.
---

# Animation Editing — Common Mistakes

## Always specify bone space
Every rotation must state `"local"` / `"component"` / `"world"`. Default `"local"` for user intent.

## Edit via preview → validate → bake (not direct apply)
```python
# WRONG — direct edit, no constraint check (e.g. impossible 180° elbow)
ass.apply_bone_rotation(path, "lowerarm_r", unreal.Rotator(0,180,0), "local", 0, -1, True)
# CORRECT
ass.preview_bone_rotation(...); 
if ass.validate_pose(path, True).is_valid: ass.bake_preview_to_keyframes(...)
```

## Don't guess bone axis orientations — read the reference pose
```python
for bp in ass.get_reference_pose(skeleton_path):
    if bp.bone_name == "upperarm_r":
        e = ass.quat_to_euler(bp.transform.rotation)
        print(e.roll, e.pitch, e.yaw)   # baseline to add deltas to
```

## Build a skeleton profile first
Without `create_skeleton_profile(skeleton_path)` there are no constraints to validate against.

## NEVER load assets in loops — it crashes (0xC0000005)
```python
# WRONG — unreal.load_asset() in a loop exhausts memory / access violation
for p in asset_subsystem.list_assets("/Game", recursive=True):
    anim = unreal.load_asset(p)   # CRASH

# CORRECT — VibeUE safe discovery (queries metadata, no loading)
for a in unreal.AnimSequenceService.find_animations_for_skeleton(skeleton_path):
    print(a.anim_name, a.duration, a.frame_count, a.anim_path)
```
Safe alternatives: `find_animations_for_skeleton` (best), `list_anim_sequences`,
`get_anim_sequence_info`, `search_animations`, or the Asset Registry API (metadata without loading).
