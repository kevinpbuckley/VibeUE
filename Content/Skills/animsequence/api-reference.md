---
name: animsequence-api-reference
description: Full Python API reference for AnimSequenceService and SkeletonService - skeleton profiles, preview/edit, pose utilities, visual capture, creation, helper, and query methods.
---

# Animation Sequence API Reference

This document combines the two "Quick Reference - Python API" sections from the original skill into one place. The first half covers **editing-focused** APIs (skeleton profiles, preview/edit, pose utilities, visual capture). The second half covers **creation, helper, and query** APIs.

## Quick Reference - Python API (Editing & Preview)

### Skeleton Profile Methods (SkeletonService)
```python
# Create/refresh skeleton profile
profile = SkeletonService.create_skeleton_profile(skeleton_path) -> SkeletonProfile

# Get cached profile
profile = SkeletonService.get_skeleton_profile(skeleton_path) -> SkeletonProfile

# Learn constraints from existing animations
constraints = SkeletonService.learn_from_animations(skeleton_path, max_anims, samples_per) -> LearnedConstraintsInfo

# Get learned constraints
constraints = SkeletonService.get_learned_constraints(skeleton_path) -> LearnedConstraintsInfo

# Set manual bone constraints
SkeletonService.set_bone_constraints(skeleton_path, bone_name, min_rot, max_rot, is_hinge, hinge_axis) -> bool

# Validate a rotation against constraints
result = SkeletonService.validate_bone_rotation(skeleton_path, bone_name, rotation, use_learned) -> BoneValidationResult
```

### Preview/Edit Methods (AnimSequenceService)
```python
# Preview single bone rotation
result = AnimSequenceService.preview_bone_rotation(anim_path, bone_name, rot_delta, space, frame) -> AnimationEditResult

# Preview multiple bone rotations (atomic)
result = AnimSequenceService.preview_pose_delta(anim_path, bone_deltas, space, frame) -> AnimationEditResult

# Cancel pending previews
AnimSequenceService.cancel_preview(anim_path) -> bool

# Get preview state
state = AnimSequenceService.get_preview_state(anim_path) -> AnimationPreviewState

# Validate current pose against constraints
result = AnimSequenceService.validate_pose(anim_path, use_learned) -> PoseValidationResult

# Bake previewed edits to keyframes
result = AnimSequenceService.bake_preview_to_keyframes(anim_path, start, end, interp) -> AnimationEditResult

# Apply rotation directly (no preview)
result = AnimSequenceService.apply_bone_rotation(anim_path, bone, rot, space, start, end, is_delta) -> AnimationEditResult
```

### Pose Utility Methods (AnimSequenceService)
```python
# Copy pose between frames/animations
result = AnimSequenceService.copy_pose(src_path, src_frame, dst_path, dst_frame, bone_filter) -> AnimationEditResult

# Mirror pose (swap left/right)
result = AnimSequenceService.mirror_pose(anim_path, frame, mirror_axis) -> AnimationEditResult

# Get skeleton reference pose
poses = AnimSequenceService.get_reference_pose(skeleton_path) -> Array[BonePose]

# Convert quaternion to Euler
euler = AnimSequenceService.quat_to_euler(quat) -> Rotator

# Preview on different skeleton
result = AnimSequenceService.retarget_preview(anim_path, target_skeleton) -> AnimationEditResult
```

### Visual Capture Methods (AnimSequenceService)

> **Default Output Folder:** `Saved/VibeUE/Screenshots/<AnimationName>/`
> 
> Pass empty string `""` for output path to use the default folder.
> The folder is automatically cleared on editor startup.

```python
# Capture a single animation pose to PNG image (AI visual feedback)
# Pass "" for output_path to use default: Saved/VibeUE/Screenshots/<AnimName>/pose_<time>.png
result = AnimSequenceService.capture_animation_pose(
    anim_path,          # str - Animation asset path
    time,               # float - Time in seconds to capture (default: 0.0)
    output_path,        # str - Output PNG path ("" = auto in Saved/VibeUE/Screenshots/)
    camera_angle,       # str - "front", "side", "back", "three_quarter", "top" (default: "front")
    image_width,        # int - Output width in pixels (default: 512)
    image_height        # int - Output height in pixels (default: 512)
) -> AnimationPoseCaptureResult

# Capture multiple frames as image sequence
# Pass "" for output_directory to use default: Saved/VibeUE/Screenshots/<AnimName>/
results = AnimSequenceService.capture_animation_sequence(
    anim_path,          # str - Animation asset path  
    output_directory,   # str - Directory for PNGs ("" = auto in Saved/VibeUE/Screenshots/)
    frame_count,        # int - Number of evenly-spaced frames to capture (default: 5)
    camera_angle,       # str - Camera angle for all frames (default: "front")
    image_width,        # int - Output width (default: 512)
    image_height        # int - Output height (default: 512)
) -> Array[AnimationPoseCaptureResult]

# Example: Capture with default folder
import unreal
results = unreal.AnimSequenceService.capture_animation_sequence(
    "/Game/Anims/AS_SwordSwing",  # Animation to capture
    "",                           # Empty = use default Saved/VibeUE/Screenshots/AS_SwordSwing/
    5,                            # 5 frames
    "front",                      # Front view
    512, 512                      # Image size
)
for r in results:
    print(f"Frame {r.captured_frame}: {r.image_path}")
```

> **⚠️ Captures come back all-black when there is no skeletal mesh to render.**
> These methods render the animation onto the skeleton's **preview mesh**. A sequence that
> animates bones but has no associated/previewable skeletal mesh (e.g. a freshly created
> `create_anim_sequence` asset, `bones=0`) produces black PNGs. Before relying on visual
> capture, confirm the animation's skeleton has a preview mesh, or capture an existing
> production animation instead. If images are black, fall back to numeric verification with
> `get_pose_at_time()` / `get_bone_transform_at_time()`.

---

## Secondary Skill Manifest (preserved from original skill.md)

> The original `skill.md` contained a second YAML manifest block mid-file (an artifact of merging two skill documents). It is preserved verbatim below so no content is lost.

```yaml
---
name: animsequence
display_name: Animation Sequences
description: Query and manipulate animation sequences, keyframes, curves, notifies, and sync markers
vibeue_classes:
  - AnimSequenceService
unreal_classes:
  - AnimSequence
  - AnimNotify
  - AnimNotifyState
keywords:
  - animation
  - sequence
  - keyframe
  - curve
  - notify
  - sync marker
  - root motion
  - pose
  - bone track
  - create animation
  - wave
  - arm raise
---
```

# Animation Sequence Skill

Use `AnimSequenceService` for animation sequence operations.

> **Related Skills:**
> - **skeleton** - For skeleton structure, sockets, and retargeting configuration
> - **animation-blueprint** - For state machines and AnimGraph navigation

## Quick Reference - Python API (Creation, Helpers, Query)

### Creation Methods
```python
# Create static animation from reference pose
anim_path = AnimSequenceService.create_from_pose(skeleton_path, anim_name, save_path, duration) -> str

# Create animation with custom keyframes
anim_path = AnimSequenceService.create_anim_sequence(skeleton_path, anim_name, save_path, duration, frame_rate, bone_tracks) -> str

# Get reference pose keyframe for a bone
keyframe = AnimSequenceService.get_reference_pose_keyframe(skeleton_path, bone_name, time) -> AnimKeyframe
```

### Helper Methods (Returns values, not out-params!)
```python
# Convert Euler angles to quaternion - RETURNS Quat (not out-param in Python!)
quat = AnimSequenceService.euler_to_quat(roll, pitch, yaw) -> Quat

# Multiply quaternions - RETURNS Quat (not out-param in Python!)
combined = AnimSequenceService.multiply_quats(a, b) -> Quat
```

### Query Methods
```python
# Get animation info - RETURNS AnimSequenceInfo (not bool with out-param!)
info = AnimSequenceService.get_anim_sequence_info(anim_path) -> AnimSequenceInfo

# Get animated bone names
bones = AnimSequenceService.get_animated_bones(anim_path) -> Array[str]

# Get bone transform at time - RETURNS Transform (not bool with out-param!)
transform = AnimSequenceService.get_bone_transform_at_time(anim_path, bone_name, time, global_space) -> Transform

# Get full pose at time
poses = AnimSequenceService.get_pose_at_time(anim_path, time, global_space) -> Array[BonePose]
```

### Key Structs
```python
# BoneTrackData - contains bone name and keyframes
track = unreal.BoneTrackData()
track.bone_name = "upperarm_r"
track.keyframes = [kf1, kf2, kf3]

# AnimKeyframe - single keyframe with transform data
kf = unreal.AnimKeyframe()
kf.time = 0.5              # Time in seconds
kf.position = unreal.Vector(0, 0, 0)
kf.rotation = unreal.Quat()  # Use euler_to_quat() to create
kf.scale = unreal.Vector(1, 1, 1)
```
