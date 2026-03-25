---
name: animsequence
display_name: Animation Sequences & Editing
description: Preview, validate, bake, and manipulate animation sequences with constraint-aware bone editing
vibeue_classes:
  - AnimSequenceService
  - SkeletonService
unreal_classes:
  - AnimSequence
  - Skeleton
  - SkeletalMesh
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
  - animation editing
  - bone rotation
  - bone space
  - local space
  - component space
  - constraint
  - joint limit
  - preview
  - validate
  - bake
  - retarget
  - skeleton profile
  - learned constraints
  - mirror
  - copy pose
  - create animation
  - swing
  - wave
  - dance
  - jump
  - walk
  - run
  - attack
  - axe swing
  - arm raise
---

# Animation Sequence & Editing Skill

This skill covers the **inspect → profile → preview → validate → bake** workflow for safe animation bone edits with correct bone-space handling and constraint validation.

> **Required:**
> ```python
> manage_skills(action="load", skill_name="animsequence")
> ```
>
> **Related Skills:**
> - **skeleton** - For modifying skeleton structure, sockets, and retargeting modes
> - **animation-blueprint** - For state machines and AnimGraph navigation
>
> **Workflow:** Create skeleton profile → Learn constraints → Preview bone rotations → Validate pose → Bake to keyframes

> **🛡️ SAFE DISCOVERY: Always use VibeUE service methods**
>
> **DO NOT** use `unreal.load_asset()` in loops - causes memory crashes!
>
> **USE THESE SAFE METHODS:**
> - `unreal.SkeletonService.list_skeletons(search_path)` - Find skeletons
> - `unreal.AnimSequenceService.find_animations_for_skeleton(skeleton_path)` - Find animations
> - `unreal.AnimSequenceService.list_anim_sequences(search_path, skeleton_filter)` - List all animations
>
> These methods query asset metadata WITHOUT loading assets into memory.
> - **animation-blueprint** - For state machines and AnimGraph navigation
>
> **Workflow:** Create skeleton profile → Learn constraints → Preview bone rotations → Validate pose → Bake to keyframes

## ⚠️ Critical Rules

### 1. Always Specify Bone Space

All bone rotations must specify the coordinate space. **Default to "local" for user intent.**

| Space | Description | Use For |
|-------|-------------|---------|
| `"local"` | Relative to parent bone | Most edits (default) |
| `"component"` | Relative to skeletal mesh root | Cross-bone coordination |
| `"world"` | World coordinates | Rarely used for animations |

```python
# WRONG: Assuming space or not specifying
unreal.AnimSequenceService.apply_bone_rotation(path, "arm", rot, ...)

# CORRECT: Always specify space explicitly
unreal.AnimSequenceService.preview_bone_rotation(
    path, "upperarm_r", unreal.Rotator(0, 30, 0), "local", 0
)
```

### 2. Use Preview → Validate → Bake Workflow

**Never apply edits directly without validation.** Use the preview workflow:

```python
import unreal

anim_path = "/Game/Anims/AS_Idle"

# Step 1: PREVIEW the edit
result = unreal.AnimSequenceService.preview_bone_rotation(
    anim_path, "upperarm_r", unreal.Rotator(0, 45, 0), "local", 0
)

# Step 2: VALIDATE against constraints
validation = unreal.AnimSequenceService.validate_pose(anim_path, True)  # Use learned constraints
if not validation.is_valid:
    for msg in validation.violation_messages:
        print(f"⚠️ {msg}")
    # Option A: Cancel and adjust
    unreal.AnimSequenceService.cancel_preview(anim_path)
    # Option B: Accept clamped values and continue

# Step 3: BAKE if valid
if validation.is_valid:
    result = unreal.AnimSequenceService.bake_preview_to_keyframes(
        anim_path, 0, -1, "cubic"
    )
    print(f"✓ Baked frames {result.start_frame} to {result.end_frame}")
```

### 3. Build Skeleton Profile Before Editing

Create a skeleton profile to get hierarchy, constraints, and learned ranges:

```python
# WRONG: Editing without understanding the skeleton
unreal.AnimSequenceService.apply_bone_rotation(...)

# CORRECT: Build profile first
profile = unreal.SkeletonService.create_skeleton_profile("/Game/SK_Mannequin")
print(f"Skeleton has {profile.bone_count} bones")

# Check if constraints are available
if not profile.has_learned_constraints:
    # Learn from existing animations
    constraints = unreal.SkeletonService.learn_from_animations("/Game/SK_Mannequin", 50, 10)
    print(f"Learned from {constraints.animation_count} animations")
```

### 4. Use Quaternions Internally, Euler for Intent

User intent is expressed in Euler angles (degrees), but always use quaternions internally to avoid gimbal lock:

```python
# User intent: "rotate forearm 30 degrees"
rotation_delta = unreal.Rotator(0, 30, 0)  # Euler (Roll, Pitch, Yaw)

# The service converts to quaternion internally
result = unreal.AnimSequenceService.preview_bone_rotation(
    anim_path, "lowerarm_r", rotation_delta, "local", 0
)

# To inspect quaternion values:
euler = unreal.AnimSequenceService.quat_to_euler(some_quat)
```

---

## Quick Reference - Python API

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

---

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

## Workflows

### 1. Inspect Skeleton and Build Profile

Before any editing, understand the skeleton:

```python
import unreal

skeleton_path = "/Game/Characters/SK_Mannequin"

# Step 1: Build skeleton profile
profile = unreal.SkeletonService.create_skeleton_profile(skeleton_path)
print(f"Skeleton: {profile.skeleton_name}")
print(f"Bones: {profile.bone_count}")

# Step 2: Inspect hierarchy for target bones
for bone in profile.bone_hierarchy:
    if "arm" in bone.bone_name.lower():
        print(f"  {bone.bone_name} (parent: {bone.parent_bone_name})")

# Step 3: Check if learned constraints exist
if profile.has_learned_constraints:
    print("Using learned constraints")
else:
    print("Learning from animations...")
    constraints = unreal.SkeletonService.learn_from_animations(skeleton_path, 50, 10)
    print(f"Analyzed {constraints.animation_count} animations")
```

### 2. Preview and Validate Single Bone Edit

Safe workflow for editing one bone:

```python
import unreal

anim_path = "/Game/Anims/AS_Idle"
skeleton_path = unreal.AnimSequenceService.get_animation_skeleton(anim_path)

# Step 1: Build profile (if not cached)
unreal.SkeletonService.create_skeleton_profile(skeleton_path)

# Step 2: Preview the rotation
result = unreal.AnimSequenceService.preview_bone_rotation(
    anim_path,
    "upperarm_r",
    unreal.Rotator(0, 45, 0),  # Raise arm 45 degrees
    "local",
    0  # Preview at frame 0
)

if not result.success:
    print(f"Preview failed: {result.error_message}")
else:
    # Step 3: Validate against constraints
    validation = unreal.AnimSequenceService.validate_pose(anim_path, True)
    
    if validation.is_valid:
        print("✓ Pose is valid, baking...")
        bake_result = unreal.AnimSequenceService.bake_preview_to_keyframes(
            anim_path, 0, -1, "cubic"
        )
        print(f"Baked frames {bake_result.start_frame}-{bake_result.end_frame}")
    else:
        print("✗ Constraint violations:")
        for msg in validation.violation_messages:
            print(f"  - {msg}")
        
        # Decide: cancel or accept clamped values
        if result.was_clamped:
            print("Accepting clamped rotation...")
            unreal.AnimSequenceService.bake_preview_to_keyframes(anim_path, 0, -1, "cubic")
        else:
            unreal.AnimSequenceService.cancel_preview(anim_path)
```

### 3. Multi-Bone Atomic Edit (e.g., Arm Chain)

Edit multiple bones together, all-or-nothing:

```python
import unreal

anim_path = "/Game/Anims/AS_Wave"

# Define deltas for arm chain
deltas = [
    unreal.BoneDelta(bone_name="clavicle_r", rotation_delta=unreal.Rotator(0, 0, 15)),
    unreal.BoneDelta(bone_name="upperarm_r", rotation_delta=unreal.Rotator(0, 90, 0)),
    unreal.BoneDelta(bone_name="lowerarm_r", rotation_delta=unreal.Rotator(0, 45, 0)),
    unreal.BoneDelta(bone_name="hand_r", rotation_delta=unreal.Rotator(0, -30, 20))
]

# Preview all at once (atomic - fails if any bone fails)
result = unreal.AnimSequenceService.preview_pose_delta(anim_path, deltas, "local", 15)

if result.success:
    # Validate entire pose
    validation = unreal.AnimSequenceService.validate_pose(anim_path, True)
    
    if validation.is_valid:
        unreal.AnimSequenceService.bake_preview_to_keyframes(anim_path, 0, -1, "cubic")
        print(f"✓ Applied {len(result.modified_bones)} bone edits")
    else:
        print(f"✗ {validation.failed_count} bones violated constraints")
        unreal.AnimSequenceService.cancel_preview(anim_path)
else:
    print(f"Preview failed: {result.error_message}")
```

### 4. Set Manual Constraints for a Bone

Define anatomical limits for a joint:

```python
import unreal

skeleton_path = "/Game/Characters/SK_Mannequin"

# Set elbow as hinge joint (only bends on pitch axis)
unreal.SkeletonService.set_bone_constraints(
    skeleton_path,
    "lowerarm_r",
    unreal.Rotator(0, 0, 0),     # Min: no hyperextension
    unreal.Rotator(0, 145, 0),   # Max: 145 degree bend
    True,  # Is hinge joint
    1      # Pitch axis (Y)
)

# Set shoulder with more freedom
unreal.SkeletonService.set_bone_constraints(
    skeleton_path,
    "upperarm_r",
    unreal.Rotator(-45, -90, -90),   # Min rotation
    unreal.Rotator(45, 180, 90),     # Max rotation
    False,  # Not a hinge
    0
)

print("Constraints set for arm bones")
```

### 5. Learn Constraints from Existing Animations

Analyze project animations to derive realistic bone ranges:

```python
import unreal

skeleton_path = "/Game/Characters/SK_Mannequin"

# Learn from up to 100 animations, 20 samples each
constraints = unreal.SkeletonService.learn_from_animations(skeleton_path, 100, 20)

print(f"Analyzed {constraints.animation_count} animations ({constraints.total_samples} samples)")

# Inspect learned ranges for specific bones
for bone_range in constraints.bone_ranges:
    if "arm" in bone_range.bone_name.lower():
        print(f"\n{bone_range.bone_name}:")
        print(f"  Range: {bone_range.min_rotation} to {bone_range.max_rotation}")
        print(f"  Safe (5%-95%): {bone_range.percentile_5} to {bone_range.percentile_95}")
        print(f"  Samples: {bone_range.sample_count}")
```

### 6. Copy Pose Between Animations

Transfer a pose from one animation to another:

```python
import unreal

# Copy frame 0 of idle to frame 30 of custom animation
result = unreal.AnimSequenceService.copy_pose(
    "/Game/Anims/AS_Idle", 0,          # Source
    "/Game/Anims/AS_Custom", 30,       # Destination
    []  # Empty = all bones
)

if result.success:
    print(f"Copied {len(result.modified_bones)} bones")
else:
    print(f"Failed: {result.error_message}")

# Copy only specific bones
arm_bones = ["upperarm_r", "lowerarm_r", "hand_r"]
result = unreal.AnimSequenceService.copy_pose(
    "/Game/Anims/AS_Wave", 15,
    "/Game/Anims/AS_Custom", 45,
    arm_bones
)
```

### 7. Mirror Pose (Swap Left/Right)

Create mirrored version of a pose:

```python
import unreal

anim_path = "/Game/Anims/AS_Wave_Right"

# Mirror frame 15 across X axis
result = unreal.AnimSequenceService.mirror_pose(anim_path, 15, "X")

if result.success:
    print(f"Mirrored {len(result.modified_bones)} bones")
    # Bones like "hand_r" now have "hand_l" transforms and vice versa
```

### 8. Preview on Different Skeleton (Retargeting)

Test how an animation looks on a different character:

```python
import unreal

result = unreal.AnimSequenceService.retarget_preview(
    "/Game/Anims/AS_Run",
    "/Game/MetaHumans/SK_MetaHuman"
)

if result.success:
    print("Retarget preview active - check Animation Editor")
else:
    print(f"Retarget failed: {result.error_message}")
    for msg in result.messages:
        print(f"  - {msg}")
```

---

## COMMON_MISTAKES

### Wrong: Applying rotations without space
```python
# WRONG - space is ambiguous
transform = unreal.Transform(rotation=some_rotation)
# This may be interpreted incorrectly

# CORRECT - always specify space
result = unreal.AnimSequenceService.preview_bone_rotation(
    anim_path, bone, rotation, "local", frame
)
```

### Wrong: Editing without validation
```python
# WRONG - direct edit without checking constraints
unreal.AnimSequenceService.apply_bone_rotation(
    path, "lowerarm_r", unreal.Rotator(0, 180, 0),  # Impossible elbow bend!
    "local", 0, -1, True
)

# CORRECT - preview → validate → bake
result = unreal.AnimSequenceService.preview_bone_rotation(...)
validation = unreal.AnimSequenceService.validate_pose(path, True)
if validation.is_valid:
    unreal.AnimSequenceService.bake_preview_to_keyframes(...)
```

### Wrong: Guessing bone axis orientations
```python
# WRONG - assuming pitch raises arm
rotation = unreal.Rotator(0, 90, 0)  # May not do what you expect!

# CORRECT - analyze reference pose first
ref_pose = unreal.AnimSequenceService.get_reference_pose(skeleton_path)
for bp in ref_pose:
    if bp.bone_name == "upperarm_r":
        euler = unreal.AnimSequenceService.quat_to_euler(bp.transform.rotation)
        print(f"Reference: Roll={euler.roll}, Pitch={euler.pitch}, Yaw={euler.yaw}")
        # Now you know the baseline to add deltas to
```

### Wrong: Not building skeleton profile
```python
# WRONG - editing without profile
unreal.AnimSequenceService.preview_bone_rotation(...)  # No constraints!

# CORRECT - build profile first
unreal.SkeletonService.create_skeleton_profile(skeleton_path)
# Now constraints are available for validation
```

### ⚠️ CRITICAL: Loading assets in loops causes crashes
```python
# WRONG - Loading assets causes memory access violations (0xC0000005)
asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
assets = asset_subsystem.list_assets("/Game", recursive=True)
for asset_path in assets:
    anim = unreal.load_asset(asset_path)  # ❌ CRASHES!
    if anim and hasattr(anim, 'get_skeleton'):
        skeleton = anim.get_skeleton()  # Memory violation

# CORRECT - Use Asset Registry API (no loading required)
asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
filter = unreal.ARFilter(
    class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "AnimSequence")],
    recursive_paths=True,
    package_paths=["/Game"]
)
anim_assets = asset_registry.get_assets(filter)
for asset_data in anim_assets:
    # Access metadata without loading
    anim_name = asset_data.asset_name
    anim_path = asset_data.object_path
    # Get skeleton path from asset registry tags
    skeleton_tag = asset_data.get_tag_value("Skeleton")
```

**Why this crashes:**
- `unreal.load_asset()` loads full asset into memory
- Loading hundreds of animations in a loop exhausts memory
- Unreal's garbage collection can't keep up
- Results in exception code 0xC0000005 (access violation)

**Safe alternatives:**
1. **USE VibeUE's `FindAnimationsForSkeleton()` - BEST OPTION**
2. Use Asset Registry API to query metadata without loading
3. Load one asset at a time with explicit unload
4. Filter by path before loading (limit scope)

**BEST: Use VibeUE AnimSequenceService methods**
```python
# CORRECT - VibeUE safe discovery (no loading)
import unreal

skeleton_path = "/Game/Characters/SK_Mannequin"

# Get all animations for a skeleton - no loading!
anims = unreal.AnimSequenceService.find_animations_for_skeleton(skeleton_path)

for anim_info in anims[:10]:
    print(f"{anim_info.anim_name}")
    print(f"  Duration: {anim_info.duration:.2f}s")
    print(f"  Frames: {anim_info.frame_count}")
    print(f"  Path: {anim_info.anim_path}")
```

**Other safe VibeUE methods:**
- `list_anim_sequences(search_path, skeleton_filter)` - List all in path
- `get_anim_sequence_info(anim_path)` - Get single anim info
- `search_animations(name_pattern, search_path)` - Find by name pattern---
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

# Animation Sequence Skill

Use `AnimSequenceService` for animation sequence operations.

> **Related Skills:**
> - **skeleton** - For skeleton structure, sockets, and retargeting configuration
> - **animation-blueprint** - For state machines and AnimGraph navigation

## Quick Reference - Python API

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

## IMPORTANT: Asset Path Format

**All `anim_path` parameters require the FULL asset path (package_name), NOT the folder path (package_path).**

When using `AssetDiscoveryService.search_assets()`, use `package_name` NOT `package_path`:

```python
import unreal

# Search for an animation
results = unreal.AssetDiscoveryService.search_assets("Run", "AnimSequence")
if results:
    asset = results[0]
    
    # CORRECT: Use package_name (full asset path)
    anim_path = str(asset.package_name)  # e.g., "/Game/Animations/Run/AS_Run_Forward"
    
    # WRONG: Do NOT use package_path (folder only)
    # folder = asset.package_path  # e.g., "/Game/Animations/Run" - This will FAIL!
    
    # Now you can use the anim_path with AnimSequenceService
    info = unreal.AnimSequenceService.get_anim_sequence_info(anim_path)
```

## Method Categories

| Category | Methods |
|----------|---------|
| Discovery | `list_anim_sequences`, `get_anim_sequence_info`, `find_animations_for_skeleton`, `search_animations` |
| **Creation** | `create_from_pose`, `create_anim_sequence`, `get_reference_pose_keyframe` |
| **Helpers** | `euler_to_quat`, `multiply_quats` |
| Properties | `get_animation_length`, `get_animation_frame_rate`, `get_animation_frame_count`, `get_animation_skeleton`, `get_rate_scale`, `set_rate_scale` |
| Bone Tracks | `get_animated_bones`, `get_bone_transform_at_time`, `get_bone_transform_at_frame` |
| Poses | `get_pose_at_time`, `get_pose_at_frame`, `get_root_motion_at_time`, `get_total_root_motion` |
| Curves | `list_curves`, `get_curve_info`, `get_curve_value_at_time`, `get_curve_keyframes`, `add_curve`, `remove_curve`, `set_curve_keys`, `add_curve_key` |
| Notifies | `list_notifies`, `get_notify_info`, `add_notify`, `add_notify_state`, `remove_notify`, `set_notify_trigger_time`, `set_notify_duration`, `set_notify_track`, `set_notify_name`, `set_notify_color`, `set_notify_trigger_chance`, `set_notify_trigger_on_server`, `set_notify_trigger_on_follower`, `set_notify_trigger_weight_threshold`, `set_notify_lod_filter` |
| Notify Tracks | `list_notify_tracks`, `get_notify_track_count`, `add_notify_track`, `remove_notify_track` |
| Sync Markers | `list_sync_markers`, `add_sync_marker`, `remove_sync_marker`, `set_sync_marker_time` |
| Root Motion | `get_enable_root_motion`, `set_enable_root_motion`, `get_root_motion_root_lock`, `set_root_motion_root_lock`, `get_force_root_lock`, `set_force_root_lock` |
| Additive | `get_additive_anim_type`, `set_additive_anim_type`, `get_additive_base_pose`, `set_additive_base_pose` |
| Compression | `get_compression_info`, `set_compression_scheme`, `compress_animation` |
| Export | `export_animation_to_json`, `get_source_files` |
| Editor | `open_animation_editor`, `set_preview_time`, `play_preview`, `stop_preview` |

All methods are static. Most use time in seconds (not frames).

## Creation Workflows

### Creating Animation from Reference Pose

Use `create_from_pose` to create a static animation from a skeleton's reference pose:

```python
import unreal

# Create a 1-second static animation from reference pose
anim_path = unreal.AnimSequenceService.create_from_pose(
    skeleton_path="/Game/Characters/Mannequin/Mesh/SK_Mannequin_Skeleton",
    anim_name="AS_Static_Pose",
    save_path="/Game/Animations",
    duration=1.0
)

if anim_path:
    unreal.log(f"Created animation: {anim_path}")
```

### Creating Animation from Keyframe Data

Use `create_anim_sequence` to create an animation with custom bone tracks and keyframes.

**IMPORTANT: Keyframe transforms are ABSOLUTE local-space values, NOT deltas from reference pose.**
- `position` - Absolute local position relative to parent bone
- `rotation` - Absolute local rotation as quaternion
- `scale` - Absolute local scale

**Recommended workflow for animated bones:**
1. Use `get_reference_pose_keyframe` to get the bone's reference pose
2. Use `euler_to_quat` to create rotation deltas in degrees
3. Use `multiply_quats` to combine reference rotation with delta

```python
import unreal

# Helper function to create a wave animation
skeleton_path = "/Game/Characters/UE5_Mannequins/Meshes/SK_Mannequin"

# Step 1: Get reference pose keyframe for the bone
ref_kf = unreal.AnimSequenceService.get_reference_pose_keyframe(skeleton_path, "upperarm_r", 0.0)

# Step 2: Create rotation delta (raise arm 90 degrees)
arm_raise = unreal.AnimSequenceService.euler_to_quat(0, 90, 0)  # Roll, Pitch, Yaw in degrees

# Step 3: Combine with reference rotation
raised_rotation = unreal.AnimSequenceService.multiply_quats(ref_kf.rotation, arm_raise)

# Step 4: Create keyframes
bone_tracks = [
    unreal.BoneTrackData(
        bone_name="upperarm_r",
        keyframes=[
            # Start at reference pose
            unreal.AnimKeyframe(
                time=0.0, 
                position=ref_kf.position, 
                rotation=ref_kf.rotation, 
                scale=ref_kf.scale
            ),
            # Arm raised at 0.5s
            unreal.AnimKeyframe(
                time=0.5, 
                position=ref_kf.position, 
                rotation=raised_rotation, 
                scale=ref_kf.scale
            ),
            # Back to reference at 1.0s
            unreal.AnimKeyframe(
                time=1.0, 
                position=ref_kf.position, 
                rotation=ref_kf.rotation, 
                scale=ref_kf.scale
            )
        ]
    )
]

# Step 5: Create the animation
anim_path = unreal.AnimSequenceService.create_anim_sequence(
    skeleton_path=skeleton_path,
    anim_name="AS_ArmRaise",
    save_path="/Game/Animations",
    duration=1.0,
    frame_rate=30.0,
    bone_tracks=bone_tracks
)

if anim_path:
    print(f"Created animation: {anim_path}")
    unreal.AnimSequenceService.open_animation_editor(anim_path)
```

### Simple Keyframe Creation (Direct Values)

For simple animations where you know the exact transforms:

```python
import unreal

skeleton_path = "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin"

# Create keyframes individually (recommended for clarity)
kf1 = unreal.AnimKeyframe()
kf1.time = 0.0
kf1.position = unreal.Vector(0, 0, 0)
kf1.rotation = unreal.Quat()  # Identity rotation
kf1.scale = unreal.Vector(1, 1, 1)

kf2 = unreal.AnimKeyframe()
kf2.time = 0.5
kf2.position = unreal.Vector(0, 0, 10)  # Move up 10 units
kf2.rotation = unreal.Quat()
kf2.scale = unreal.Vector(1, 1, 1)

kf3 = unreal.AnimKeyframe()
kf3.time = 1.0
kf3.position = unreal.Vector(0, 0, 0)
kf3.rotation = unreal.Quat()
kf3.scale = unreal.Vector(1, 1, 1)

# Create bone track
track = unreal.BoneTrackData()
track.bone_name = "pelvis"
track.keyframes = [kf1, kf2, kf3]

# Create animation
anim_path = unreal.AnimSequenceService.create_anim_sequence(
    skeleton_path,
    "AS_Pelvis_Bounce",
    "/Game/Animations",
    1.0,   # duration
    30.0,  # frame_rate
    [track]
)

if anim_path:
    print(f"Created: {anim_path}")
    unreal.EditorAssetLibrary.save_asset(anim_path)
```

### Creating Rotation Keyframes

Use `euler_to_quat()` to convert degrees to quaternions:

```python
import unreal

skeleton_path = "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin"

# Helper function for rotation keyframes
def make_rotation_kf(time, roll, pitch, yaw):
    kf = unreal.AnimKeyframe()
    kf.time = time
    kf.position = unreal.Vector(0, 0, 0)
    # euler_to_quat RETURNS a Quat in Python (not out-param!)
    kf.rotation = unreal.AnimSequenceService.euler_to_quat(roll, pitch, yaw)
    kf.scale = unreal.Vector(1, 1, 1)
    return kf

# Create rotation animation (spine twist)
track = unreal.BoneTrackData()
track.bone_name = "spine_01"
track.keyframes = [
    make_rotation_kf(0.0, 0, 0, 0),     # Start: no rotation
    make_rotation_kf(0.5, 0, 0, 45),    # Middle: 45° yaw twist
    make_rotation_kf(1.0, 0, 0, 0),     # End: back to start
]

anim_path = unreal.AnimSequenceService.create_anim_sequence(
    skeleton_path,
    "AS_Spine_Twist",
    "/Game/Animations",
    1.0,
    30.0,
    [track]
)
```

### Wave Animation Example (Multi-Bone)

> **🛑 STOP! Do NOT skip this section!**
>
> Creating bone rotation animations **WILL FAIL** if you guess axis values.
> You MUST complete the discovery workflow BEFORE writing any rotation code.

#### COMMON MISTAKES (Why Previous Attempts Failed)

| Mistake | Why It Fails | Fix |
|---------|--------------|-----|
| Assuming Pitch raises arm | Bone local axes ≠ world axes | Run discovery to find actual "raise" axis |
| Using (0,0,0) as idle | Reference pose is NOT zeros | Get actual values from `get_reference_pose()` |
| Guessing rotation values | Every skeleton is different | Analyze existing animations first |
| Changing multiple axes at once | Can't tell which caused the effect | Test ONE axis at a time |

#### Required Workflow (DO NOT SKIP)

**Step 1: Discover reference pose values**
```python
import unreal

skeleton_path = "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin"  # Your skeleton
target_bones = ["upperarm_r", "lowerarm_r", "hand_r"]

# Get the REST position for each bone
pose = unreal.AnimSequenceService.get_reference_pose(skeleton_path)
for bp in pose:
    if bp.bone_name in target_bones:
        euler = unreal.AnimSequenceService.quat_to_euler(bp.transform.rotation)
        print(f"{bp.bone_name}: Roll={euler.x:.1f}, Pitch={euler.y:.1f}, Yaw={euler.z:.1f}")
```

**Step 2: Find an existing animation that raises an arm and analyze it**
```python
# Find animations to analyze
anims = unreal.AnimSequenceService.find_animations_for_skeleton(skeleton_path)
for a in anims[:20]:
    print(f"{a.anim_name}: {a.duration}s")

# Compare idle pose vs raised pose in an existing animation
anim_path = "/Game/Path/To/Animation"  # Use an animation that moves the arm
for time in [0.0, 0.5, 1.0]:
    pose = unreal.AnimSequenceService.get_pose_at_time(anim_path, time, False)
    for bp in pose:
        if bp.bone_name == "upperarm_r":
            euler = unreal.AnimSequenceService.quat_to_euler(bp.transform.rotation)
            print(f"t={time}: Roll={euler.x:.1f}, Pitch={euler.y:.1f}, Yaw={euler.z:.1f}")
```

**Step 3: Test which axis raises the arm (change ONE at a time)**
```python
# Create test animation changing ONLY Roll
# If arm doesn't go up, try Pitch, then Yaw
ref = (2.7, -37.8, 0.2)  # Replace with YOUR reference pose values

# Test Roll change
test_roll = (-90.0, ref[1], ref[2])  # Only Roll changed

# Test Pitch change (if Roll didn't work)
test_pitch = (ref[0], 45.0, ref[2])  # Only Pitch changed

# Test Yaw change (if others didn't work)  
test_yaw = (ref[0], ref[1], 90.0)  # Only Yaw changed
```

**Step 4: Only after discovery, create the wave animation**
```python
import unreal

# REPLACE these with YOUR discovered values from Steps 1-3
skeleton_path = "/Game/Path/To/Your/Skeleton"

def make_kf(time, roll, pitch, yaw):
    kf = unreal.AnimKeyframe()
    kf.time = time
    kf.position = unreal.Vector(0, 0, 0)
    kf.rotation = unreal.AnimSequenceService.euler_to_quat(roll, pitch, yaw)
    kf.scale = unreal.Vector(1, 1, 1)
    return kf

# FROM DISCOVERY - Replace with your actual values!
idle_upperarm = (0.0, 0.0, 0.0)       # From get_reference_pose()
raised_upperarm = (-90.0, 0.0, 45.0)  # From testing which axis raises arm

idle_lowerarm = (0.0, 0.0, 0.0)       # From get_reference_pose()
bent_lowerarm = (0.0, 0.0, 60.0)      # From testing which axis bends elbow

idle_hand = (0.0, 0.0, 0.0)           # From get_reference_pose()
wave_left = (0.0, 25.0, 0.0)          # From testing which axis tilts hand
wave_right = (0.0, -25.0, 0.0)

# Create bone tracks
upperarm = unreal.BoneTrackData()
upperarm.bone_name = "upperarm_r"  # Adjust bone name for your skeleton
upperarm.keyframes = [
    make_kf(0.0, *idle_upperarm),     # Start at rest
    make_kf(0.3, *raised_upperarm),   # Arm raised
    make_kf(1.5, *raised_upperarm),   # Hold raised
    make_kf(2.0, *idle_upperarm),     # Return to rest
]

# Lower arm track - bend elbow
lowerarm = unreal.BoneTrackData()
lowerarm.bone_name = "lowerarm_r"  # Adjust bone name for your skeleton
lowerarm.keyframes = [
    make_kf(0.0, *idle_lowerarm),
    make_kf(0.3, *bent_lowerarm),
    make_kf(1.5, *bent_lowerarm),
    make_kf(2.0, *idle_lowerarm),
]

# Hand track - wave side to side
hand = unreal.BoneTrackData()
hand.bone_name = "hand_r"  # Adjust bone name for your skeleton
hand.keyframes = [
    make_kf(0.0, *idle_hand),
    make_kf(0.3, *idle_hand),      # Hold while arm raises
    make_kf(0.5, *wave_left),
    make_kf(0.7, *wave_right),
    make_kf(0.9, *wave_left),
    make_kf(1.1, *wave_right),
    make_kf(1.3, *wave_left),
    make_kf(1.5, *wave_right),
    make_kf(2.0, *idle_hand),      # Return to rest
]

# STEP 6: Create the animation
anim_path = unreal.AnimSequenceService.create_anim_sequence(
    skeleton_path,
    "AS_WaveHello",
    "/Game/Animations",
    2.0,   # Duration in seconds
    30.0,  # Frame rate
    [upperarm, lowerarm, hand]
)

if anim_path:
    print(f"✓ Created wave animation: {anim_path}")
    unreal.EditorAssetLibrary.save_asset(anim_path)
```

## Common Patterns

### Listing Animations for a Skeleton

```python
import unreal

# Find all animations for a specific skeleton
anims = unreal.AnimSequenceService.find_animations_for_skeleton(
    "/Game/Characters/Mannequin/Mesh/SK_Mannequin_Skeleton"
)

for anim in anims:
    print(f"{anim.anim_name}: {anim.duration}s, {anim.frame_count} frames")
```

### Getting Bone Pose at Time

```python
import unreal

# Get full skeleton pose at 0.5 seconds
pose = unreal.AnimSequenceService.get_pose_at_time(
    "/Game/Animations/Run",
    0.5,
    True  # Get global space transforms
)

for bone_pose in pose:
    print(f"{bone_pose.bone_name}:")
    print(f"  Location: {bone_pose.transform.location}")
    print(f"  Rotation: {bone_pose.transform.rotation}")
```

### Adding Animation Curves

```python
import unreal

# Add a morph target curve
unreal.AnimSequenceService.add_curve(
    "/Game/Animations/Facial",
    "jaw_open",
    True  # Is morph target
)

# Add keys to the curve
unreal.AnimSequenceService.add_curve_key(
    "/Game/Animations/Facial",
    "jaw_open",
    0.0,  # Time
    0.0   # Value (closed)
)

unreal.AnimSequenceService.add_curve_key(
    "/Game/Animations/Facial",
    "jaw_open",
    0.5,  # Time
    1.0   # Value (open)
)
```

### Managing Animation Notifies

**IMPORTANT**: Always get the full asset path using `package_name` from search results!

**Display Name Behavior:**
- **Instant notifies with custom names**: Use base `/Script/Engine.AnimNotify` class - creates "skeleton notifies" that display your custom name in the editor timeline
- **Instant notifies without names**: Creates skeleton notify (base AnimNotify is abstract in UE5.7+), editor displays "Notify"
- **State notifies**: MUST use a concrete subclass (NOT the abstract base class). Custom names are stored in `NotifyName` but the editor displays the class name

**⚠️ COMMON MISTAKES:**
- ❌ Using `/Script/Engine.AnimNotifyState` - **FAILS** (abstract class, cannot instantiate)
- ❌ Using custom notify class + custom name - editor shows class name, not your custom name
- ✅ Use `/Script/Engine.AnimNotify` with a custom name for named instant notifies (creates skeleton notify)
- ✅ Use concrete subclasses like `/Script/Engine.AnimNotify_PlaySound` for functional notifies
- ✅ Use concrete subclasses like `/Script/Engine.AnimNotifyState_Trail` for state notifies

```python
import unreal

# Step 1: Find an animation and get the FULL asset path
results = unreal.AssetDiscoveryService.search_assets("Run", "AnimSequence")
if not results:
    print("No animations found")
else:
    # CRITICAL: Use package_name (full path), NOT package_path (folder only)
    anim_path = str(results[0].package_name)
    print(f"Using animation: {anim_path}")
    
    # List existing notifies
    notifies = unreal.AnimSequenceService.list_notifies(anim_path)
    print(f"Found {len(notifies)} existing notifies:")
    for n in notifies:
        print(f"  [{n.notify_index}] {n.notify_name} @ {n.trigger_time:.3f}s (state={n.is_state})")
    
    # Add an instant notify with custom name (displays "LeftFoot" in editor)
    idx = unreal.AnimSequenceService.add_notify(
        anim_path,
        "/Script/Engine.AnimNotify",  # Base class + custom name = skeleton notify
        0.25,                          # Trigger time in seconds
        "LeftFoot"                     # Custom name - displayed in editor
    )
    if idx >= 0:
        print(f"Created instant notify at index {idx}")
    
    # NOTE: State notifies require a CONCRETE subclass, NOT the abstract base class
    # Example using AnimNotifyState_Trail (if available in your project)
    # state_idx = unreal.AnimSequenceService.add_notify_state(
    #     anim_path,
    #     "/Script/Engine.AnimNotifyState_Trail",  # Concrete class, NOT AnimNotifyState
    #     0.1,                                       # Start time
    #     0.3,                                       # Duration
    #     "SwordTrail"                               # Optional name
    # )
    
    # Modify notify timing
    unreal.AnimSequenceService.set_notify_trigger_time(anim_path, idx, 0.5)
    
    # Modify notify track (visual row in editor)
    unreal.AnimSequenceService.set_notify_track(anim_path, idx, 1)
    
    # Rename a notify (for base AnimNotify, this also converts to skeleton notify for proper display)
    unreal.AnimSequenceService.set_notify_name(anim_path, idx, "RightFoot")
    
    # Get notify info
    info = unreal.AnimSequenceService.get_notify_info(anim_path, idx)
    if info:
        print(f"Notify: {info.notify_name} @ {info.trigger_time}s, track {info.track_index}")
    
    # Remove a notify
    unreal.AnimSequenceService.remove_notify(anim_path, idx)
```

### Configuring Notify Behavior

```python
import unreal

anim_path = "/Game/Animations/MyAnim"

# Rename a notify (changes display name in editor)
# For base AnimNotify class, this also converts to skeleton notify for proper display
unreal.AnimSequenceService.set_notify_name(anim_path, 0, "Footstep_Left")

# Set editor display color using LinearColor object
orange = unreal.LinearColor(1.0, 0.5, 0.0, 1.0)
unreal.AnimSequenceService.set_notify_color(anim_path, 0, orange)

# Set trigger chance (0.0-1.0, for random triggering)
unreal.AnimSequenceService.set_notify_trigger_chance(anim_path, 0, 0.75)  # 75% chance

# Control triggering on dedicated server (default True)
unreal.AnimSequenceService.set_notify_trigger_on_server(anim_path, 0, False)  # Disable on server

# Control triggering on sync group followers (default True)
unreal.AnimSequenceService.set_notify_trigger_on_follower(anim_path, 0, False)

# Set minimum blend weight to trigger (0.0-1.0)
unreal.AnimSequenceService.set_notify_trigger_weight_threshold(anim_path, 0, 0.5)  # Only trigger at 50%+ weight

# Set LOD filtering ("NoFiltering" or "LOD" with level 0-3)
unreal.AnimSequenceService.set_notify_lod_filter(anim_path, 0, "LOD", 2)  # Only LOD 0-2

# Read all notify properties via get_notify_info
info = unreal.AnimSequenceService.get_notify_info(anim_path, 0)
if info:
    print(f"Name: {info.notify_name}")
    print(f"Trigger Chance: {info.trigger_chance}")
    print(f"Trigger On Server: {info.trigger_on_server}")
    print(f"Trigger On Follower: {info.trigger_on_follower}")
    print(f"Weight Threshold: {info.trigger_weight_threshold}")
    print(f"Filter Type: {info.notify_filter_type}")
    print(f"Filter LOD: {info.notify_filter_lod}")
```

## CRITICAL RULES

1. **Full Asset Path Required**: Use `package_name` from AssetData (e.g., `/Game/Folder/AssetName`), NOT `package_path` (which is just the folder)
2. **Skeleton Required**: All creation methods require a valid skeleton path
3. **Asset Path Format**: Use `/Game/` format, not file paths
4. **Time Units**: Most methods use seconds, not frames
5. **Frame Rate**: Default is 30 FPS if not specified
6. **Bone Names**: Must match exact bone names from skeleton
7. **Save Before Use**: Newly created animations are automatically saved
8. **Keyframe Times**: Must be within [0, duration] range
9. **Reference Pose**: `create_from_pose` uses skeleton's reference pose
10. **Notify Class Paths**: `add_notify` and `add_notify_state` require FULL class paths:
   - Instant notify: `/Script/Engine.AnimNotify`
   - State notify: `/Script/Engine.AnimNotifyState`
   - Sound notify: `/Script/Engine.AnimNotify_PlaySound`
   - Custom: `/Script/YourModule.YourNotifyClass`
11. **Bone Rotation Axes Are Non-Intuitive**: Roll/Pitch/Yaw do NOT map predictably to world-space movement. Each bone's local coordinate system is different. **Always discover axis mappings** using `get_reference_pose()` and `get_pose_at_time()` before creating rotation animations!

## Bone Rotation Axis Discovery

> **⚠️ CRITICAL WARNING**: Bone local coordinate systems in UE5 are **non-intuitive**!
> 
> - Roll, Pitch, and Yaw meanings depend on how each bone is oriented in the skeleton
> - What looks like "arm up" might be Roll on one skeleton and Pitch on another
> - Different skeletons (Mannequin, MetaHuman, custom) have different bone orientations
> - **NEVER assume** axis mappings - always discover them first!

### Why This Matters

Common mistake: Assuming Pitch rotates an arm up/down (like airplane pitch). In reality:
- Bones have local coordinate systems based on their orientation in the skeleton
- A bone pointing along X-axis will rotate differently than one pointing along Y-axis
- The only reliable way to know is to **test and observe**

### Discovery Workflow (Required Before Rotation Animations)

**Step 1: Get reference pose values** - Find the "rest" rotation for target bones:

```python
import unreal

skeleton_path = "/Game/Path/To/Your/Skeleton"
target_bones = ["upperarm_r", "lowerarm_r", "hand_r"]  # Bones you want to animate

pose = unreal.AnimSequenceService.get_reference_pose(skeleton_path)
for bp in pose:
    if bp.bone_name in target_bones:
        q = bp.transform.rotation
        euler = unreal.AnimSequenceService.quat_to_euler(q)
        print(f"{bp.bone_name}: Roll={euler.x:.1f}, Pitch={euler.y:.1f}, Yaw={euler.z:.1f}")
```

**Step 2: Analyze existing animations** - See what axes change during desired movement:

```python
# Find an animation that does what you want (e.g., arm raise, wave)
idle_pose = unreal.AnimSequenceService.get_pose_at_time("/Game/Anims/Idle", 0.0, False)
action_pose = unreal.AnimSequenceService.get_pose_at_time("/Game/Anims/ArmRaise", 0.5, False)

# Compare euler angles for target bones
for bone_name in target_bones:
    idle_bone = next((b for b in idle_pose if b.bone_name == bone_name), None)
    action_bone = next((b for b in action_pose if b.bone_name == bone_name), None)
    if idle_bone and action_bone:
        idle_e = unreal.AnimSequenceService.quat_to_euler(idle_bone.transform.rotation)
        action_e = unreal.AnimSequenceService.quat_to_euler(action_bone.transform.rotation)
        print(f"{bone_name}:")
        print(f"  Roll:  {idle_e.x:.1f} -> {action_e.x:.1f} (delta: {action_e.x - idle_e.x:.1f})")
        print(f"  Pitch: {idle_e.y:.1f} -> {action_e.y:.1f} (delta: {action_e.y - idle_e.y:.1f})")
        print(f"  Yaw:   {idle_e.z:.1f} -> {action_e.z:.1f} (delta: {action_e.z - idle_e.z:.1f})")
```

**Step 3: Test incrementally** - Change ONE axis at a time to confirm its effect:

```python
# Create test animation varying only one axis
# Example: Test if Roll controls arm elevation
ref_roll, ref_pitch, ref_yaw = 2.7, -37.8, 0.2  # From reference pose

test_keyframes = [
    (0.0, ref_roll, ref_pitch, ref_yaw),        # Start at reference
    (0.5, ref_roll - 45, ref_pitch, ref_yaw),   # Change Roll only
    (1.0, ref_roll - 90, ref_pitch, ref_yaw),   # Change Roll more
]
# Create animation, preview, observe which direction the bone moves
```

### Key Insight

The axis that controls a particular movement varies by:
- **Skeleton source** (Mannequin vs MetaHuman vs custom)
- **Bone orientation** in the skeleton hierarchy
- **Local vs component space** transforms

**Always run the discovery workflow** before writing rotation animation code. Document your findings for each skeleton you work with.

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
