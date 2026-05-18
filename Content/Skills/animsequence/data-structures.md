---
name: animsequence-data-structures
description: Key data structures (FSkeletonProfile, FBoneConstraint, FLearnedBoneRange, edit/capture results, FBoneDelta) and full DTO reference (AnimSequenceInfo, BoneTrackData, AnimKeyframe, BonePose, AnimCurveInfo, CurveKeyframe, AnimNotifyInfo, SyncMarkerInfo) for animation sequence work.
---

# Animation Sequence Data Structures

## Key Data Structures

### FSkeletonProfile
```python
profile.skeleton_path       # str - Asset path
profile.skeleton_name       # str - Display name
profile.bone_count          # int - Number of bones
profile.is_valid            # bool - Whether profile is built
profile.has_learned_constraints  # bool - Whether learned data exists
profile.bone_hierarchy      # Array[BoneNodeInfo] - Hierarchy
profile.constraints         # Array[BoneConstraint] - User constraints
profile.learned_ranges      # Array[LearnedBoneRange] - From animations
profile.retarget_profiles   # Array[str] - Retarget profile names
```

### FBoneConstraint
```python
constraint.bone_name        # str - Bone this applies to
constraint.min_rotation     # Rotator - Minimum angles (degrees)
constraint.max_rotation     # Rotator - Maximum angles (degrees)
constraint.is_hinge         # bool - Single-axis rotation only
constraint.hinge_axis       # int - 0=X, 1=Y, 2=Z
constraint.rotation_order   # str - Euler order (default "YXZ")
```

### FLearnedBoneRange
```python
range.bone_name             # str - Bone name
range.min_rotation          # Rotator - Observed minimum
range.max_rotation          # Rotator - Observed maximum
range.percentile_5          # Rotator - 5th percentile (safe min)
range.percentile_95         # Rotator - 95th percentile (safe max)
range.sample_count          # int - Number of samples
```

### FAnimationEditResult
```python
result.success              # bool - Whether edit succeeded
result.modified_bones       # Array[str] - Bones that changed
result.start_frame          # int - Start of affected range
result.end_frame            # int - End of affected range
result.was_clamped          # bool - Whether constraint clamping occurred
result.messages             # Array[str] - Warnings/info
result.error_message        # str - Error if failed
```

### FAnimationPoseCaptureResult
```python
result.success              # bool - Whether capture succeeded
result.image_path           # str - Full path to saved PNG
result.anim_path            # str - Animation that was captured
result.captured_time        # float - Time in seconds that was captured
result.captured_frame       # int - Frame number that was captured
result.image_width          # int - Output image width in pixels
result.image_height         # int - Output image height in pixels
result.camera_angle         # str - Camera angle used ("front", "side", etc.)
result.error_message        # str - Error if capture failed
```

### FBoneDelta (for multi-bone edits)
```python
delta = unreal.BoneDelta()
delta.bone_name = "upperarm_r"
delta.rotation_delta = unreal.Rotator(0, 30, 0)
delta.translation_delta = unreal.Vector(0, 0, 0)  # Optional
delta.scale_delta = unreal.Vector(1, 1, 1)        # Multiplicative
```

---

## Data Transfer Objects (DTOs)

> **IMPORTANT: Python Naming Conventions**
>
> Unreal's Python bindings convert C++ property names:
> - `bSomething` (C++ bool prefix) → `something` (Python, no prefix)
> - `CamelCase` → `camel_case` (Python snake_case)
> - `FCurveKeyframe` (C++ struct) → `CurveKeyframe` (Python, no F prefix)
>
> **ALWAYS use discovery tools to verify property names when unsure!**

### AnimSequenceInfo
- `anim_path` - Full path to animation asset
- `anim_name` - Asset name
- `skeleton_path` - Skeleton asset path
- `duration` - Length in seconds
- `frame_rate` - Frames per second
- `frame_count` - Total frame count
- `bone_track_count` - Number of animated bones
- `curve_count` - Number of curves
- `notify_count` - Number of notifies
- `enable_root_motion` - Root motion flag (NOT `b_enable_root_motion`)
- `additive_anim_type` - "None", "LocalSpace", "MeshSpace"
- `rate_scale` - Playback speed multiplier
- `compressed_size` - Size in bytes
- `raw_size` - Uncompressed size in bytes

### BoneTrackData
- `bone_name` - Name of the bone
- `keyframes` - Array of `AnimKeyframe`

### AnimKeyframe
- `time` - Time in seconds
- `position` - Position as Vector (not 'location')
- `rotation` - Rotation as Quat (not Rotator!)
- `scale` - Scale as Vector

### BonePose
- `bone_name` - Bone name
- `bone_index` - Index in skeleton
- `parent_index` - Parent bone index
- `transform` - Transform with location/rotation/scale

### AnimCurveInfo
- `curve_name` - Curve name
- `curve_type` - "Float", "Transform", "Morph"
- `key_count` - Number of keyframes
- `morph_target` - Whether drives morph target (NOT `b_is_morph_target`)
- `material` - Whether drives material parameter

### CurveKeyframe
- `time` - Time in seconds
- `value` - Curve value
- `interp_mode` - "Constant", "Linear", "Cubic"
- `tangent_mode` - "Auto", "User", "Break"
- `arrive_tangent` - Incoming tangent
- `leave_tangent` - Outgoing tangent

### AnimNotifyInfo
- `notify_index` - Index of the notify in the sequence
- `notify_name` - Notify display name
- `notify_class` - Full class path (e.g., "/Script/Engine.AnimNotify")
- `trigger_time` - Time in seconds when notify fires
- `duration` - Duration in seconds (0 for instant notifies)
- `is_state` - True if this is a state notify (has duration)
- `track_index` - Track row index in editor's notify panel
- `notify_color` - Color displayed in editor (LinearColor)
- `trigger_chance` - Random trigger probability (0.0-1.0, default 1.0)
- `trigger_on_server` - Whether to trigger on dedicated servers
- `trigger_on_follower` - Whether to trigger on sync group followers
- `trigger_weight_threshold` - Minimum blend weight to trigger (0.0-1.0)
- `notify_filter_type` - "AlwaysTrigger" or "LOD"
- `notify_filter_lod` - LOD level (0-3, only used if filter_type is "LOD")

### SyncMarkerInfo
- `marker_name` - Sync marker name
- `time` - Time in seconds
