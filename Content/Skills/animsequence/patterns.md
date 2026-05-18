---
name: animsequence-patterns
description: Common patterns for animation work (listing animations for a skeleton, getting bone pose at time, adding curves, managing and configuring notifies) plus the CRITICAL RULES checklist for creation, notify class paths, asset path formats, time/frame units, and bone axes.
---

# Animation Sequence Patterns

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
