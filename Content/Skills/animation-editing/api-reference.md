---
name: animation-editing/api-reference
description: Animation editing Python API quick reference — SkeletonService profile/constraint methods, AnimSequenceService preview/edit/pose methods, and the key data structures (SkeletonProfile, BoneConstraint, LearnedBoneRange, AnimationEditResult, BoneDelta).
---

# Animation Editing — API Reference

## Contents
- Skeleton profile methods (SkeletonService)
- Preview/edit methods (AnimSequenceService)
- Pose utility methods (AnimSequenceService)
- Data structures

## Skeleton profile methods (SkeletonService)

```python
SkeletonService.create_skeleton_profile(skeleton_path) -> SkeletonProfile
SkeletonService.get_skeleton_profile(skeleton_path) -> SkeletonProfile
SkeletonService.learn_from_animations(skeleton_path, max_anims, samples_per) -> LearnedConstraintsInfo
SkeletonService.get_learned_constraints(skeleton_path) -> LearnedConstraintsInfo
SkeletonService.set_bone_constraints(skeleton_path, bone_name, min_rot, max_rot, is_hinge, hinge_axis) -> bool
SkeletonService.validate_bone_rotation(skeleton_path, bone_name, rotation, use_learned) -> BoneValidationResult
```

## Preview/edit methods (AnimSequenceService)

```python
preview_bone_rotation(anim_path, bone_name, rotation_delta, space, preview_frame) -> AnimationEditResult
preview_pose_delta(anim_path, bone_deltas, space, frame) -> AnimationEditResult   # atomic multi-bone
cancel_preview(anim_path) -> bool
get_preview_state(anim_path) -> AnimationPreviewState
validate_pose(anim_path, use_learned_constraints) -> PoseValidationResult
bake_preview_to_keyframes(anim_path, start_frame, end_frame, interp_mode) -> AnimationEditResult
apply_bone_rotation(anim_path, bone, rot, space, start, end, is_delta) -> AnimationEditResult  # no preview
```

`space` is `"local"` (default for user intent), `"component"`, or `"world"`. `interp_mode` e.g. `"cubic"`.
`end_frame` of `-1` means "to the end".

## Pose utility methods (AnimSequenceService)

```python
copy_pose(src_path, src_frame, dst_path, dst_frame, bone_filter) -> AnimationEditResult  # [] filter = all bones
mirror_pose(anim_path, frame, mirror_axis) -> AnimationEditResult   # axis "X"/"Y"/"Z"
get_reference_pose(skeleton_path) -> Array[BonePose]
quat_to_euler(quat) -> Rotator
retarget_preview(anim_path, target_skeleton) -> AnimationEditResult
```

## Safe discovery (no asset loading)

```python
AnimSequenceService.find_animations_for_skeleton(skeleton_path) -> Array[AnimSequenceInfo]
AnimSequenceService.list_anim_sequences(search_path, skeleton_filter) -> Array[AnimSequenceInfo]
AnimSequenceService.get_anim_sequence_info(anim_path) -> AnimSequenceInfo
AnimSequenceService.search_animations(name_pattern, search_path) -> Array[AnimSequenceInfo]
SkeletonService.list_skeletons(search_path) -> Array
```
`AnimSequenceInfo`: `anim_name`, `anim_path`, `duration`, `frame_count`.

## Data structures

**SkeletonProfile**: `skeleton_path`, `skeleton_name`, `bone_count`, `is_valid`,
`has_learned_constraints`, `bone_hierarchy` (Array[BoneNodeInfo]), `constraints`, `learned_ranges`,
`retarget_profiles`.

**BoneConstraint**: `bone_name`, `min_rotation`, `max_rotation`, `is_hinge`, `hinge_axis` (0=X,1=Y,2=Z),
`rotation_order` (default "YXZ").

**LearnedBoneRange**: `bone_name`, `min_rotation`, `max_rotation`, `percentile_5`, `percentile_95`,
`sample_count`.

**AnimationEditResult**: `success`, `modified_bones`, `start_frame`, `end_frame`, `was_clamped`,
`messages`, `error_message`.

**BoneDelta** (multi-bone edits):
```python
delta = unreal.BoneDelta()
delta.bone_name = "upperarm_r"
delta.rotation_delta = unreal.Rotator(0, 30, 0)
delta.translation_delta = unreal.Vector(0, 0, 0)   # optional
delta.scale_delta = unreal.Vector(1, 1, 1)         # multiplicative
```
