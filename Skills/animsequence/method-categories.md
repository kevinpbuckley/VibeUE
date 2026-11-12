---
name: animsequence-method-categories
description: Categorised table of every AnimSequenceService method by domain (Discovery, Creation, Helpers, Properties, Bone Tracks, Poses, Curves, Notifies, Notify Tracks, Sync Markers, Compression, Export, Editor), plus the plain asset properties that belong to Epic's ObjectToolset.
---

# AnimSequenceService Method Categories

## Method Categories

| Category | Methods |
|----------|---------|
| Discovery | `list_anim_sequences`, `get_anim_sequence_info`, `find_animations_for_skeleton`, `search_animations` |
| **Creation** | `create_from_pose`, `create_anim_sequence`, `get_reference_pose_keyframe` |
| **Helpers** | `euler_to_quat`, `multiply_quats` |
| Properties | `get_animation_frame_rate`, `get_animation_frame_count`, `set_animation_frame_rate` |
| Bone Tracks | `get_animated_bones`, `get_bone_transform_at_time`, `get_bone_transform_at_frame` |
| Poses | `get_pose_at_time`, `get_pose_at_frame`, `get_root_motion_at_time`, `get_total_root_motion` |
| Curves | `list_curves`, `get_curve_info`, `get_curve_value_at_time`, `get_curve_keyframes`, `add_curve`, `remove_curve`, `set_curve_keys`, `add_curve_key` |
| Notifies | `list_notifies`, `get_notify_info`, `add_notify`, `add_notify_state`, `remove_notify`, `set_notify_trigger_time`, `set_notify_duration`, `set_notify_track`, `set_notify_name`, `set_notify_color`, `set_notify_trigger_chance`, `set_notify_trigger_on_server`, `set_notify_trigger_on_follower`, `set_notify_trigger_weight_threshold`, `set_notify_lod_filter` |
| Notify Tracks | `list_notify_tracks`, `get_notify_track_count`, `add_notify_track`, `remove_notify_track` |
| Sync Markers | `list_sync_markers`, `add_sync_marker`, `remove_sync_marker`, `set_sync_marker_time` |
| Compression | `get_compression_info`, `compress_animation` |
| Export | `export_animation_to_json`, `get_source_files` |
| Editor | `set_preview_time`, `play_preview`, `stop_preview` |

All methods are static. Most use time in seconds (not frames).

## Plain asset properties — Epic's `ObjectToolset`, not this service

These are ordinary `UAnimSequence` UPROPERTYs. Read and write them with
`EditorToolset.ObjectToolset.GetProperties` / `SetProperties`; Epic's write goes through
`PostEditChangeProperty`, so the engine's own fixups fire (VibeUE's old raw setters skipped them).

| Want | Property (on the AnimSequence) | Value shape |
|---|---|---|
| Skeleton | `Skeleton` | object path (read it; `VisibleAnywhere`, so do not set it) |
| Length in seconds | `SequenceLength` | float, read-only (VisibleAnywhere; do not set) |
| Playback rate | `RateScale` | float |
| Additive type | `AdditiveAnimType` | `AAT_None` / `AAT_LocalSpaceBase` / `AAT_RotationOffsetMeshSpace` |
| Additive base pose | `RefPoseType` + `RefPoseSeq` (+ `RefFrameIndex`) | `ABPT_RefPose` / `ABPT_AnimScaled` / `ABPT_AnimFrame` / `ABPT_LocalAnimFrame`; `RefPoseSeq` takes `{"refPath": "/Game/.../Base.Base"}` |
| Root motion on/off | `bEnableRootMotion` | bool |
| Root lock | `RootMotionRootLock` | `RefPose` / `AnimFirstFrame` / `Zero` |
| Force root lock | `bForceRootLock` | bool |
| Compression scheme | `BoneCompressionSettings` | `{"refPath": "/Engine/.../ABCS_X.ABCS_X"}` |

Enum literals are the engine identifiers; when unsure, `ObjectToolset.GetPropertySchema` on the
asset lists the legal values. Open the Animation Editor with
`EditorToolset.EditorAppToolset.OpenEditorForAsset(assetPath=...)`.

```python
# inside execute_python
import json
OT = "EditorToolset.ObjectToolset."
anim = {"refPath": "/Game/Anims/Run.Run"}          # object path: asset name repeated after the dot

before = json.loads(await call_tool(OT + "GetProperties", instance=anim,
                    properties=["Skeleton", "RateScale", "bEnableRootMotion", "RootMotionRootLock"]))
ok = await call_tool(OT + "SetProperties", instance=anim, values=json.dumps({
    "RateScale": 1.25, "bEnableRootMotion": True, "RootMotionRootLock": "AnimFirstFrame"}))
await call_tool("EditorToolset.AssetToolset.SaveAsset", assetPath="/Game/Anims/Run")
{"before": before, "set": ok}
```
