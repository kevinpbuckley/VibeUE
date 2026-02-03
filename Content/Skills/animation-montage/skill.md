---
name: animation-montage
display_name: Animation Montages
description: Create and manipulate animation montages with sections, slots, segments, branching points, and blend settings
vibeue_classes:
  - AnimMontageService
unreal_classes:
  - AnimMontage
  - AnimNotify
  - AnimNotifyState
keywords:
  - montage
  - animation
  - section
  - slot
  - segment
  - branching point
  - blend
  - combo
  - attack
  - root motion
  - notify
---

# Animation Montage Skill

Use `AnimMontageService` for animation montage operations.

## Quick Reference - Python API

### Discovery Methods
```python
# List all montages in a path
montages = AnimMontageService.list_montages(search_path, skeleton_filter) -> Array[MontageInfo]

# Get detailed montage info
info = AnimMontageService.get_montage_info(montage_path) -> MontageInfo

# Find montages for a skeleton
montages = AnimMontageService.find_montages_for_skeleton(skeleton_path) -> Array[MontageInfo]

# Find montages using a specific animation
montages = AnimMontageService.find_montages_using_animation(anim_path) -> Array[MontageInfo]
```

### Creation Methods
```python
# Create montage from an animation sequence
path = AnimMontageService.create_montage_from_animation(anim_path, dest_path, name) -> str

# Create empty montage for a skeleton
path = AnimMontageService.create_empty_montage(skeleton_path, dest_path, name) -> str

# Duplicate existing montage
path = AnimMontageService.duplicate_montage(source_path, dest_path, new_name) -> str
```

### Section Management (CRITICAL for gameplay!)
```python
# List all sections
sections = AnimMontageService.list_sections(montage_path) -> Array[MontageSectionInfo]

# Add new section
AnimMontageService.add_section(montage_path, section_name, start_time) -> bool

# Remove section
AnimMontageService.remove_section(montage_path, section_name) -> bool

# Rename section
AnimMontageService.rename_section(montage_path, old_name, new_name) -> bool

# Move section to new time
AnimMontageService.set_section_start_time(montage_path, section_name, new_time) -> bool

# Get section at time
name = AnimMontageService.get_section_name_at_time(montage_path, time) -> str
```

### Section Linking (Combo/Flow Control)
```python
# Link sections: Section1 -> Section2
AnimMontageService.set_next_section(montage_path, from_section, to_section) -> bool

# Set section to loop
AnimMontageService.set_section_loop(montage_path, section_name, loop=True) -> bool

# Clear link (section ends montage)
AnimMontageService.clear_section_link(montage_path, section_name) -> bool

# Get all section links for flow visualization
links = AnimMontageService.get_all_section_links(montage_path) -> Array[SectionLink]
```

### Animation Segments (Multiple Animations in One Montage)
```python
# List segments in a slot track
segments = AnimMontageService.list_anim_segments(montage_path, track_index) -> Array[AnimSegmentInfo]

# Add animation at specific time
index = AnimMontageService.add_anim_segment(montage_path, track_index, anim_path, start_time, play_rate) -> int

# Remove segment
AnimMontageService.remove_anim_segment(montage_path, track_index, segment_index) -> bool

# Modify segment timing
AnimMontageService.set_segment_start_time(montage_path, track_index, segment_index, time) -> bool
AnimMontageService.set_segment_play_rate(montage_path, track_index, segment_index, rate) -> bool
AnimMontageService.set_segment_loop_count(montage_path, track_index, segment_index, count) -> bool
```

### Slot Tracks
```python
# List slot tracks
tracks = AnimMontageService.list_slot_tracks(montage_path) -> Array[SlotTrackInfo]

# Add new slot track
index = AnimMontageService.add_slot_track(montage_path, slot_name) -> int

# Change slot name
AnimMontageService.set_slot_name(montage_path, track_index, new_slot_name) -> bool

# Remove slot track
AnimMontageService.remove_slot_track(montage_path, track_index) -> bool
```

### Blend Settings
```python
# Set blend in (options: Linear, Cubic, HermiteCubic, Sinusoidal, etc.)
AnimMontageService.set_blend_in(montage_path, time, option) -> bool

# Set blend out
AnimMontageService.set_blend_out(montage_path, time, option) -> bool

# Set blend out trigger time (-1 = auto)
AnimMontageService.set_blend_out_trigger_time(montage_path, time) -> bool

# Get all blend settings
settings = AnimMontageService.get_blend_settings(montage_path) -> MontageBlendSettings
```

### Branching Points (Frame-Accurate Gameplay Events)
```python
# List all branching points
points = AnimMontageService.list_branching_points(montage_path) -> Array[BranchingPointInfo]

# Add branching point for combo input window
index = AnimMontageService.add_branching_point(montage_path, name, trigger_time) -> int

# Remove branching point
AnimMontageService.remove_branching_point(montage_path, index) -> bool
```

### Root Motion
```python
# Enable/disable root motion translation
AnimMontageService.set_enable_root_motion_translation(montage_path, enable) -> bool

# Enable/disable root motion rotation
AnimMontageService.set_enable_root_motion_rotation(montage_path, enable) -> bool
```

### Editor Methods
```python
# Open montage in Animation Editor
AnimMontageService.open_montage_editor(montage_path) -> bool

# Refresh editor UI after modifications (IMPORTANT!)
AnimMontageService.refresh_montage_editor(montage_path) -> bool

# Jump to section in preview
AnimMontageService.jump_to_section(montage_path, section_name) -> bool

# Play preview
AnimMontageService.play_preview(montage_path, start_section) -> bool
```

## IMPORTANT: Asset Path Format

**All `montage_path` parameters require the FULL asset path (package_name), NOT the folder path.**

```python
# CORRECT: Full asset path
montage_path = "/Game/Montages/AM_Attack"

# WRONG: Folder path
montage_path = "/Game/Montages"  # This is a folder, not an asset!
```

## Key Workflows

### Create Multi-Animation Montage with Sections

```python
import unreal

# 1. Create empty montage
path = unreal.AnimMontageService.create_empty_montage(
    "/Game/Characters/SK_Mannequin",
    "/Game/Montages",
    "AM_ComboAttack"
)

# 2. Add animation segments sequentially (track 0)
unreal.AnimMontageService.add_anim_segment(path, 0, "/Game/Animations/Attack1", 0.0)
unreal.AnimMontageService.add_anim_segment(path, 0, "/Game/Animations/Attack2", 1.0)  
unreal.AnimMontageService.add_anim_segment(path, 0, "/Game/Animations/Attack3", 2.0)

# 3. Add sections at animation boundaries
unreal.AnimMontageService.add_section(path, "Attack1", 0.0)
unreal.AnimMontageService.add_section(path, "Attack2", 1.0)
unreal.AnimMontageService.add_section(path, "Attack3", 2.0)

# 4. Link sections for combo flow
unreal.AnimMontageService.set_next_section(path, "Attack1", "Attack2")
unreal.AnimMontageService.set_next_section(path, "Attack2", "Attack3")
# Attack3 has no next section - ends the montage

# 5. Add branching points for input windows
unreal.AnimMontageService.add_branching_point(path, "Combo1Window", 0.7)
unreal.AnimMontageService.add_branching_point(path, "Combo2Window", 1.7)

# 6. Configure blend settings
unreal.AnimMontageService.set_blend_in(path, 0.15, "Cubic")
unreal.AnimMontageService.set_blend_out(path, 0.2, "Linear")

# 7. IMPORTANT: Refresh editor to see changes
unreal.AnimMontageService.refresh_montage_editor(path)

# 8. Save
unreal.EditorAssetLibrary.save_asset(path)
```

### Create Layered Upper Body Montage

```python
import unreal

# 1. Create montage from animation
path = unreal.AnimMontageService.create_montage_from_animation(
    "/Game/Animations/Reload",
    "/Game/Montages",
    "AM_Reload_UpperBody"
)

# 2. Change slot to UpperBody for layered blending
unreal.AnimMontageService.set_slot_name(path, 0, "UpperBody")

# 3. Configure blend
unreal.AnimMontageService.set_blend_in(path, 0.1, "Linear")
unreal.AnimMontageService.set_blend_out(path, 0.15, "Linear")

# 4. Disable root motion for upper body action
unreal.AnimMontageService.set_enable_root_motion_translation(path, False)
unreal.AnimMontageService.set_enable_root_motion_rotation(path, False)

# 5. Save
unreal.EditorAssetLibrary.save_asset(path)
```

### Set Up Looping Idle Section

```python
import unreal

montage_path = "/Game/Montages/AM_Combat"

# Add sections
unreal.AnimMontageService.add_section(montage_path, "Idle", 0.0)
unreal.AnimMontageService.add_section(montage_path, "Attack", 2.0)

# Make Idle loop until gameplay interrupts
unreal.AnimMontageService.set_section_loop(montage_path, "Idle", True)

# Attack ends the montage (no next section)
unreal.AnimMontageService.clear_section_link(montage_path, "Attack")
```

## COMMON_MISTAKES

### Wrong: Using folder path instead of asset path
```python
# WRONG
montages = AnimMontageService.list_sections("/Game/Montages")

# CORRECT
sections = AnimMontageService.list_sections("/Game/Montages/AM_Attack")
```

### Wrong: Not refreshing editor after modifications
```python
# WRONG - Changes may not be visible
AnimMontageService.add_section(path, "Attack", 0.5)
# Editor shows stale data

# CORRECT - Always refresh after modifications
AnimMontageService.add_section(path, "Attack", 0.5)
AnimMontageService.refresh_montage_editor(path)  # Now UI is updated
```

### Wrong: Adding sections beyond montage duration
```python
# WRONG - Section time must be within montage length
# If montage is 3.0s, this fails:
AnimMontageService.add_section(path, "Late", 5.0)  # Beyond duration

# CORRECT - Add segments first to extend duration, then add sections
AnimMontageService.add_anim_segment(path, 0, anim_path, 3.0)  # Extends montage
AnimMontageService.add_section(path, "Late", 3.5)  # Now within range
```

### Wrong: Forgetting to save
```python
# WRONG - Changes lost on editor restart
AnimMontageService.add_section(path, "Attack", 0.5)

# CORRECT - Always save after changes
AnimMontageService.add_section(path, "Attack", 0.5)
unreal.EditorAssetLibrary.save_asset(path)
```
