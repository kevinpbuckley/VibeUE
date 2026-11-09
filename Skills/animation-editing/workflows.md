---
name: animation-editing/workflows
description: Step-by-step animation editing workflows — build a skeleton profile, preview/validate/bake single and multi-bone edits, set manual constraints, learn constraints from animations, copy/mirror poses, and retarget preview.
---

# Animation Editing — Workflows

## Contents
- Inspect skeleton and build profile
- Preview and validate a single bone edit
- Multi-bone atomic edit
- Set manual constraints for a bone
- Learn constraints from existing animations
- Copy pose between animations
- Mirror pose (swap left/right)
- Preview on a different skeleton (retargeting)

The core safe loop is **profile → preview → validate → bake** (or `cancel_preview` if invalid).
Runnable examples under `scripts/`.

## Inspect skeleton and build profile

```python
import unreal
skeleton_path = "/Game/Characters/SK_Mannequin"
profile = unreal.SkeletonService.create_skeleton_profile(skeleton_path)
print(profile.skeleton_name, profile.bone_count)
for bone in profile.bone_hierarchy:
    if "arm" in bone.bone_name.lower():
        print(bone.bone_name, "parent:", bone.parent_bone_name)
if not profile.has_learned_constraints:
    unreal.SkeletonService.learn_from_animations(skeleton_path, 50, 10)
```

## Preview and validate a single bone edit

```python
import unreal
ass = unreal.AnimSequenceService
anim_path = "/Game/Anims/AS_Idle"
skeleton_path = ass.get_animation_skeleton(anim_path)
unreal.SkeletonService.create_skeleton_profile(skeleton_path)

r = ass.preview_bone_rotation(anim_path, "upperarm_r", unreal.Rotator(0, 45, 0), "local", 0)
if r.success:
    v = ass.validate_pose(anim_path, True)
    if v.is_valid:
        ass.bake_preview_to_keyframes(anim_path, 0, -1, "cubic")
    else:
        for m in v.violation_messages: print(m)
        ass.cancel_preview(anim_path)   # or accept clamped values if r.was_clamped
```

Runnable: `scripts/preview_validate_bake.txt`.

## Multi-bone atomic edit

```python
import unreal
ass = unreal.AnimSequenceService
deltas = [
    unreal.BoneDelta(bone_name="clavicle_r", rotation_delta=unreal.Rotator(0, 0, 15)),
    unreal.BoneDelta(bone_name="upperarm_r", rotation_delta=unreal.Rotator(0, 90, 0)),
    unreal.BoneDelta(bone_name="lowerarm_r", rotation_delta=unreal.Rotator(0, 45, 0)),
]
r = ass.preview_pose_delta("/Game/Anims/AS_Wave", deltas, "local", 15)   # all-or-nothing
if r.success and ass.validate_pose("/Game/Anims/AS_Wave", True).is_valid:
    ass.bake_preview_to_keyframes("/Game/Anims/AS_Wave", 0, -1, "cubic")
```

Runnable: `scripts/multi_bone_edit.txt`.

## Set manual constraints for a bone

```python
import unreal
sks = unreal.SkeletonService
skp = "/Game/Characters/SK_Mannequin"
# Elbow as hinge (pitch axis only), 0..145 degrees
sks.set_bone_constraints(skp, "lowerarm_r", unreal.Rotator(0,0,0), unreal.Rotator(0,145,0), True, 1)
# Shoulder with more freedom
sks.set_bone_constraints(skp, "upperarm_r", unreal.Rotator(-45,-90,-90), unreal.Rotator(45,180,90), False, 0)
```

## Learn constraints from existing animations

```python
import unreal
c = unreal.SkeletonService.learn_from_animations("/Game/Characters/SK_Mannequin", 100, 20)
print(c.animation_count, c.total_samples)
for r in c.bone_ranges:
    if "arm" in r.bone_name.lower():
        print(r.bone_name, r.percentile5, "->", r.percentile95)
```

Runnable: `scripts/learn_constraints.txt`.

## Copy pose between animations

```python
import unreal
unreal.AnimSequenceService.copy_pose("/Game/Anims/AS_Idle", 0, "/Game/Anims/AS_Custom", 30, [])  # [] = all bones
```

## Mirror pose (swap left/right)

```python
import unreal
unreal.AnimSequenceService.mirror_pose("/Game/Anims/AS_Wave_Right", 15, "X")
```

Runnable: `scripts/mirror_pose.txt`.

## Preview on a different skeleton (retargeting)

```python
import unreal
r = unreal.AnimSequenceService.retarget_preview("/Game/Anims/AS_Run", "/Game/MetaHumans/SK_MetaHuman")
print(r.success, list(r.messages))
```
