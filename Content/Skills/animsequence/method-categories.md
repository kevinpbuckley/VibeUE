---
name: animsequence-method-categories
description: Categorised table of every AnimSequenceService method by domain (Discovery, Creation, Helpers, Properties, Bone Tracks, Poses, Curves, Notifies, Notify Tracks, Sync Markers, Root Motion, Additive, Compression, Export, Editor).
---

# AnimSequenceService Method Categories

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
