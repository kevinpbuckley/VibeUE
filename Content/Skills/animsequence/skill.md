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

# Animation Sequence Skill

Use `AnimSequenceService` for animation sequence operations.

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
