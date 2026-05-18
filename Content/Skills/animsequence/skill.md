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

---

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

---

## Sub-docs available

Load these sibling docs for deeper coverage of specific topics:

- **api-reference.md** - Full Python API reference (SkeletonService profile/constraint methods, AnimSequenceService preview/edit/pose/visual capture, creation/helper/query methods, key structs). Also contains the secondary skill manifest for the merged "Animation Sequence Skill" content set.
- **data-structures.md** - Key data structures (FSkeletonProfile, FBoneConstraint, FLearnedBoneRange, FAnimationEditResult, FAnimationPoseCaptureResult, FBoneDelta) and full DTO reference (AnimSequenceInfo, BoneTrackData, AnimKeyframe, BonePose, AnimCurveInfo, CurveKeyframe, AnimNotifyInfo, SyncMarkerInfo).
- **workflows.md** - Editing workflows (inspect, preview/validate single bone, multi-bone atomic, manual constraints, learn from animations, copy/mirror pose, retarget preview) and creation workflows (from reference pose, from keyframe data, simple keyframes, rotation keyframes, wave animation example).
- **patterns.md** - Common patterns (listing animations, getting pose at time, adding curves, managing/configuring notifies) and the CRITICAL RULES checklist for creation/notify operations and asset paths.
- **method-categories.md** - Categorised table of every AnimSequenceService method by domain (Discovery, Creation, Helpers, Properties, Bone Tracks, Poses, Curves, Notifies, Notify Tracks, Sync Markers, Root Motion, Additive, Compression, Export, Editor).
- **bone-rotation.md** - Bone Rotation Axis Discovery: why bone local coordinate systems are non-intuitive and the required discovery workflow (reference pose values → analyze existing animations → incremental axis testing) before writing rotation animation code.
