---
name: cinematics
display_name: Cinematics (Level Sequences)
description: Author, play, scrub and capture Unreal Engine 5.9 Level Sequences from Python - which of Epic's nine in-editor Sequencer skills to load for bindings, cameras and cuts, keys, events, Control Rig and FBX, plus the runtime half they leave out (the sequence player, scrubbing and capturing in PIE) and the pitfalls (audio overrunning, ending on a not-playing frame, root-motion meshes vanishing). Use for "build a cutscene", "add a camera cut", "key a transform", "retime a sequence", "fade the audio", "scrub to a frame and screenshot", and on the errors "MovieSceneObjectBindingID has no attribute guid", "Cannot find the requested movie scene binding", "Cannot nativize 'float' as 'MovieSceneTimeWarpVariant'", "Failed to find property 'manual_ease_in_duration'", "LevelSequenceActor has no attribute get_sequence_player", "set_current_focal_length", "Section does not have a start frame", "MovieScene has no attribute get_display_rate".
unreal_classes:
  - LevelSequence
  - LevelSequenceEditorSubsystem
  - MovieSceneBindingProxy
  - MovieSceneSequencePlaybackParams
  - LevelSequencePlayer
  - LevelSequenceActor
  - CineCameraComponent
engine_toolsets:
  - LevelSequenceEditor.LevelSequenceEditorSubsystem
  - SequencerScripting.MovieSceneSequenceExtensions
  - AnimationAssistantToolset.SequencerDirectorToolset
  - SceneCaptureToolset.SceneCaptureToolset
keywords:
  - sequencer
  - level sequence
  - cinematic
  - cutscene
  - camera cut
  - keyframe
  - scrub
---

# Cinematics: Level Sequences from Python (UE 5.9)

VibeUE adds no Sequencer service: Epic ships the authoring surface and nine Sequencer skills inside the
editor. This pack tells you which of those to load, gives the Python names that agents guessed wrong
(about 28 failed calls in one 48-hour jam, where 133 scripts touched MovieScene), and adds the runtime
half Epic's skills omit: playing, scrubbing and capturing a sequence in PIE. Calls marked
**(not run)** were read in the 5.9 headers or Epic's skill text; everything else ran in that jam.

## 1. Error -> cure

| Error | Cause and what to write instead |
|---|---|
| `'MovieSceneObjectBindingID' object has no attribute 'guid'`, `Cannot nativize 'MovieSceneObjectBindingID' as 'Guid'` | A binding ID is an opaque struct (guid + sequence ID), neither a Guid nor a binding. Make one with `seq.get_binding_id(binding)` or `root_seq.get_portable_binding_id(dest_seq, binding)`; turn one back into a binding with `root_seq.resolve_binding_id(bid)` **(not run)**. |
| `MovieSceneBindingProxy: Cannot find the requested movie scene binding` | The proxy points at nothing: `seq.find_binding_by_name` matches the internal name, not the display name; `convert_to_custom_binding` / `convert_to_spawnable` return NEW proxies; the sequence was recreated. Test `binding.is_valid()` and re-read `seq.get_bindings()` after any structural change. |
| `Cannot nativize 'float' as 'MovieSceneTimeWarpVariant'` | An animation section's `params.play_rate` is a variant: `v = unreal.MovieSceneTimeWarpVariant(); v.set_fixed_play_rate(0.7)`; read with `v.to_fixed_play_rate()`. |
| `MovieSceneEasingSettings: Failed to find property 'manual_ease_in_duration'` | The easing fields are not exposed: `section.set_ease_in_duration(ticks)` / `set_ease_out_duration(ticks)` **(not run)**. Tick-resolution frames, not display frames. |
| `MovieSceneMarkedFrame ... 'frame_number' ... is read-only`, `no attribute 'comment'` | `seq.add_marked_frame_range_to_sequence(unreal.FrameNumber(s), unreal.FrameNumber(e), unreal.MovieSceneTimeUnit.DISPLAY_RATE, "label")`, or `m.set_editor_property("frame_number", ...)` then `seq.add_marked_frame_to_sequence(m, unit)`; comment by index with `seq.set_marked_frame_comment(i, text)` **(not run)**. |
| `'LevelSequenceActor' object has no attribute 'get_sequence_player'` | The player is a property: `lsa.get_editor_property("sequence_player")`. |
| `'CineCameraComponent' object has no attribute 'set_current_focal_length'` / `'set_current_aperture'` | Properties: `cc.set_editor_property("current_focal_length", 35.0)`, `"current_aperture"`. |
| `Section does not have a start frame` | Sections can be unbounded: check `sec.has_start_frame()` / `has_end_frame()` before `get_start_frame()` / `get_end_frame()`. |
| `'MovieScene' object has no attribute 'get_display_rate'` | Call it on the sequence asset: `seq.get_display_rate()`, `seq.get_tick_resolution()`. |
| `'SoundWave' object has no attribute 'get_duration'` | `snd.get_editor_property("duration")` (seconds). |

## 2. Load Epic's Sequencer skills first

They are Python-registered skills, so their paths are object paths. All nine start with
`/AnimationAssistantToolset/Python/animation_toolset/skills/` (as listed by `ListSkills` in 5.9):

| Path suffix | Load it for |
|---|---|
| `sequencer_core_PY.SequencerCoreSkill` | Start here: sequence lifecycle, playback, time, track classes, property tracks, folders, marks |
| `sequencer_bindings_PY.SequencerBindingsSkill` | Possessable / spawnable / replaceable bindings, tags, converting, Dynamic Binding |
| `sequencer_camera_PY.SequencerCameraSkill` | Cameras, the camera cut track, depth of field, shakes |
| `sequencer_keyframing_PY.SequencerKeyframingSkill` | Channels, keys, interpolation, defaults, the Curve Editor |
| `sequencer_gameplay_PY.SequencerGameplaySkill` | Event tracks, triggering sequences from gameplay, blending with an AnimBP, conditions |
| `director_PY.SequencerDirectorSkill` | The Director Blueprint: event endpoints, binding resolvers |
| `sequencer_control_rig_PY.SequencerControlRigSkill` | Control Rig tracks, anim layers, space switching |
| `sequencer_import_export_PY.SequencerImportExportSkill` | FBX in and out, baking to an AnimSequence |
| `sequencer_anim_tools_PY.SequencerAnimToolsSkill` | Motion trails, Time Posing, edit pivot |

```python
docs = await call_tool("ToolsetRegistry.AgentSkillToolset.GetSkills", skillPaths=[
    "/AnimationAssistantToolset/Python/animation_toolset/skills/sequencer_core_PY.SequencerCoreSkill",
    "/AnimationAssistantToolset/Python/animation_toolset/skills/sequencer_camera_PY.SequencerCameraSkill"])
docs
```

Epic's skills name tools such as `SequencerScripting.MovieSceneSequenceExtensions`,
`SequencerScripting.MovieSceneSectionExtensions`, `SequencerScripting.MovieSceneBindingExtensions`,
`LevelSequenceEditor.LevelSequenceEditorSubsystem` and `AnimationAssistantToolset.SequencerDirectorToolset`.
In Python most are methods on the object, snake_case: `MovieSceneSectionExtensions.SetRange(section, s, e)`
is `section.set_range(s, e)`. UMG widget animation is `WidgetAnimationToolset` (see `umg-widgets`).

## 3. Time: four units in one API

- **Display-rate frames (int):** `set_playback_start` / `set_playback_end`, `section.set_range(s, e)`,
  `set_start_frame`, `set_end_frame`, `get_start_frame`, `get_end_frame`. End frames are exclusive.
- **Explicit unit:** `channel.add_key(unreal.FrameNumber(f), value, 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE)`,
  `key.get_time(unit).frame_number.value`, marked frames. Always pass `DISPLAY_RATE`.
- **Tick-resolution frames:** `params.start_frame_offset`, ease durations. Seconds to ticks:
  `t = seq.get_tick_resolution(); int(round(sec * t.numerator / t.denominator))`.
- **Seconds:** `set_view_range_start` / `end`, `set_work_range_start` / `end`; the player's
  `get_current_time()` is a qualified frame time: `.time.frame_number.value * rate.denominator / rate.numerator`
  with `rate = player.get_frame_rate()`.

Set the display rate first (`seq.set_display_rate(unreal.FrameRate(30, 1))`) and count every frame in it:
`set_range` and `add_key` convert display frames to ticks when called. Do not change the tick resolution.

## 4. Bindings

- A **binding proxy** (`unreal.MovieSceneBindingProxy`) is what `seq.get_bindings()`,
  `seq.add_possessable(actor)` and the spawnable calls return, and what tracks hang off
  (`binding.add_track(cls)`, `binding.get_tracks()`). A **`MovieSceneObjectBindingID`** is only for places
  that store a reference: camera cut sections and runtime overrides.
- `lse = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)`: its spawnable, camera and
  bound-object calls need the sequence OPEN in Sequencer.
- After changing a spawnable's live actor, `lse.save_default_spawnable_state(binding)` or the change is
  lost. Sequencer auto-adds tracks you did not ask for (lens tracks on a camera, light colour and intensity,
  Control Rig on a MetaHuman), keyed at class defaults, and they override the template: key or remove them.
- **Stand-ins gameplay replaces:** author them as spawnables, convert to a replaceable binding and tag it;
  at run time `lsa.set_binding_by_tag("Tag", [actor], False)` before `play()`. An overridden spawnable is
  never spawned; a possessable placeholder stays rendered where it stands. Never let two tags in one
  sequence resolve to the same actor.

## 5. Cameras and tracks

- **Camera cut:** `cut_section.set_camera_binding_id(seq.get_binding_id(camera_binding))` for a camera in
  the same sequence; `root.get_portable_binding_id(owning_seq, camera_binding)` when it lives in a
  sub-sequence. `unreal.MovieSceneSectionExtensions.set_camera_cut_binding(section, binding_proxy,
  root_sequence)` exists too **(not run)**.
- A camera is placed by its **transform keys**, never by the spawn pose.
- CineCamera properties that work on `actor.get_cine_camera_component()`: `current_focal_length`,
  `current_aperture`, `constrain_aspect_ratio`, `post_process_blend_weight`; structs read-modify-write:
  `filmback`, `focus_settings` (`focus_method = unreal.CameraFocusMethod.MANUAL`, `manual_focus_distance`).
  As property tracks on the camera COMPONENT the paths are `CurrentFocalLength`, `CurrentAperture`,
  `FocusSettings.ManualFocusDistance`; a property track needs `track.set_property_name_and_path(label,
  path)` or its keys move nothing.
- **Animation** goes on the skeletal mesh COMPONENT binding, assigned through the section's `params`
  (get, set `animation`, set back). **Visibility** keys drive `bHiddenInGame`: they show in PIE, not in the
  editor viewport. Cut under black: change camera while a fade is at 1.
- Save with `unreal.EditorAssetLibrary.save_loaded_asset(seq, False)` (False saves even when the edit did
  not mark the package dirty).

## 6. Audio sections: the pitfalls

- **An audio section must not outlive its sequence.** A section that ends after the playback end logs
  "playing past section end" and the sound is cut by whatever follows the sequence; a section shorter than
  its sound stops dead on the cut. Before saving, compare every audio section's end with
  `seq.get_playback_end()` and the sound's `duration`, and fade the tail on the section's volume channel.
- Time shots from measured line lengths (`snd.get_editor_property("duration")`), not a words-per-second
  estimate: recorded lines ran longer and overran into the next shot until every cut was re-timed. End beds
  and music a few frames before the sequence end, with a fade (0.8 s worked).

## 7. Playing at run time, scrubbing and capturing in PIE

- **Existing actor:** `player = lsa.get_editor_property("sequence_player")`, `lsa.get_sequence()`.
  **New player:** `player, lsa = unreal.LevelSequencePlayer.create_level_sequence_player(world, seq,
  unreal.MovieSceneSequencePlaybackSettings())`, then any `set_binding_by_tag` BEFORE `player.play()`.
- **Scrub:** build `unreal.MovieSceneSequencePlaybackParams()`, set `frame =
  unreal.FrameTime(unreal.FrameNumber(f))` with `position_type = unreal.MovieScenePositionType.FRAME` (or
  `time` with `.TIME`), `update_method = unreal.UpdatePositionMethod.SCRUB` (or `.JUMP`), then
  `player.set_playback_position(params)`. Step in from a few frames earlier so cuts and poses evaluate
  forward, and let it settle about 1 s (temporal AA, streaming) before a capture.

```python
import unreal, vibeue

seq = unreal.load_asset("/Game/Cinematics/LS_Intro")      # load assets BEFORE PIE starts
shots = []
async with vibeue.pie_session(1280, 720) as s:
    player, lsa = unreal.LevelSequencePlayer.create_level_sequence_player(
        s["world"], seq, unreal.MovieSceneSequencePlaybackSettings())
    for f in (0, 90, 150):                                   # display-rate frames
        params = unreal.MovieSceneSequencePlaybackParams()
        params.frame = unreal.FrameTime(unreal.FrameNumber(f))
        params.position_type = unreal.MovieScenePositionType.FRAME
        params.update_method = unreal.UpdatePositionMethod.SCRUB
        player.set_playback_position(params)
        await asyncio.sleep(1.0)
        shots.append(await vibeue.capture_game("C:/Proj/Saved/Shots/intro_{:04d}.png".format(f)))
    player = lsa = None                                      # drop PIE objects before the session stops
shots
```

- **"Finished" is not "not playing".** A flow that ended its beat on the first tick the player was not
  playing cut a scene short after a one-frame hitch on a web build. End at the sequence's end time, or
  after the player has been stopped for well over a second; do not end a sequence on a frame where nothing
  plays.
- **Root motion far from the actor origin needs `AlwaysTickPoseAndRefreshBones`.** A clip that carries a
  character metres from its actor leaves the bounds behind; with `OnlyTickPoseWhenRendered` the mesh stops
  updating and vanishes mid-shot. On every skeletal mesh component of such actors set
  `visibility_based_anim_tick_option = unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES`
  (on the Blueprint's component defaults, so it survives).

## 8. Verify by capture

A call that returns has written keys; it has not made a shot. After every edit that changes what is on
screen, capture the start, middle and end of each shot and both sides of every cut, then look at the
images: the wrong camera (the same framing on both sides of a cut), a subject out of frame, a stand-in
beside the real actor, a character missing. For MetaHumans, visibility tracks and binding overrides, only
PIE is the truth: in the editor viewport grooms and faces lag the Sequencer-driven body. Read numbers back
too (`sec.get_start_frame()`, key times and values) and return them as the script's final expression.
