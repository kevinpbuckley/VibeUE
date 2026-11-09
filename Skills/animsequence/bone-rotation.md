---
name: animsequence-bone-rotation
description: Bone Rotation Axis Discovery - why bone local coordinate systems are non-intuitive, and the required discovery workflow (reference pose values, analyze existing animations, incremental axis testing) before writing rotation animation code.
---

# Bone Rotation Axis Discovery

> **⚠️ CRITICAL WARNING**: Bone local coordinate systems in UE5 are **non-intuitive**!
> 
> - Roll, Pitch, and Yaw meanings depend on how each bone is oriented in the skeleton
> - What looks like "arm up" might be Roll on one skeleton and Pitch on another
> - Different skeletons (Mannequin, MetaHuman, custom) have different bone orientations
> - **NEVER assume** axis mappings - always discover them first!

## Why This Matters

Common mistake: Assuming Pitch rotates an arm up/down (like airplane pitch). In reality:
- Bones have local coordinate systems based on their orientation in the skeleton
- A bone pointing along X-axis will rotate differently than one pointing along Y-axis
- The only reliable way to know is to **test and observe**

## Discovery Workflow (Required Before Rotation Animations)

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

## Key Insight

The axis that controls a particular movement varies by:
- **Skeleton source** (Mannequin vs MetaHuman vs custom)
- **Bone orientation** in the skeleton hierarchy
- **Local vs component space** transforms

**Always run the discovery workflow** before writing rotation animation code. Document your findings for each skeleton you work with.
