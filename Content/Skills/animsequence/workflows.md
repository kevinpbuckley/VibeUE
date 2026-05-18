---
name: animsequence-workflows
description: Editing workflows (inspect skeleton, preview/validate single bone, multi-bone atomic edit, manual constraints, learn from animations, copy/mirror pose, retarget preview) and creation workflows (from reference pose, from keyframe data, simple keyframes, rotation keyframes, wave animation example).
---

# Animation Sequence Workflows

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
