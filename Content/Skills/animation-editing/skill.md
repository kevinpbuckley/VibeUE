---
name: animation-editing
display_name: Animation Editing & Bone Space Correctness
description: Preview, validate, and bake bone rotation edits with constraint awareness and retarget safety
vibeue_classes:
  - AnimSequenceService
  - SkeletonService
unreal_classes:
  - AnimSequence
  - Skeleton
  - SkeletalMesh
keywords:
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
  - keyframe
  - retarget
  - skeleton profile
  - learned constraints
  - pose
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

# Animation Editing Skill

This skill covers the **inspect → profile → preview → validate → bake** workflow for safe animation bone edits with correct bone-space handling and constraint validation.

> **⚠️ Use BOTH skills together for animation creation and editing**
>
> **Required:**
> ```python
> manage_skills(action="load", skill_names=["animation-editing", "animsequence"])
> ```
>
> **Related Skills:**
> - **animsequence** - For basic keyframe creation (load together)
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
- `search_animations(name_pattern, search_path)` - Find by name pattern
