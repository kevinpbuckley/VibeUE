# AnimSequence Test Prompts

Combined test suite for `AnimSequenceService` — discovery, bone/pose extraction, notifies, curves,
sync markers, root motion, additive/editor, and animation creation. Run sections sequentially.

Reference: use animation sequences in the project, or create test animations. Creation tests use the
**SK_UEFN_Mannequin** skeleton.

---

# Section 1 — Discovery

## 1.01 - List All Animation Sequences
List all animation sequences in /Game and show their names and durations.

## 1.02 - Get Animation Sequence Info
Get detailed information about a specific animation sequence. Show: Duration, Frame rate, Frame count,
Bone track count, Curve count, Notify count, Root motion enabled status.

## 1.03 - Find Animations for Skeleton
Find all animation sequences that use the SK_UEFN_Mannequin skeleton. List their names and durations.

## 1.04 - Search Animations by Pattern
Search for all animations with "Walk" in the name. List matching animations.

## 1.05 - Search Animations by Pattern (Wildcard)
Search for animations matching "*Run*" pattern. Show results.

## 1.06 - Get Animation Properties
For a specific animation: (1) length in seconds, (2) frame rate, (3) frame count, (4) skeleton path,
(5) rate scale.

## 1.07 - Get Animated Bones
List all bones that have animation data in a specific animation sequence.

## 1.08 - Get Bone Tracks
Get all bone tracks in an animation and show: Bone name, Position key count, Rotation key count,
Scale key count.

## 1.09 - Compare Two Animations
Compare two animation sequences: (1) list both with properties, (2) which has more frames, (3) more
curves, (4) more notifies.

## 1.10 - List Animations with Root Motion
Find all animations that have root motion enabled. List their names and total root motion displacement.

---

# Section 2 — Bone Tracks and Pose Extraction

## 2.01 - Get Bone Transform at Time
Get the transform of the "hand_r" bone at time 0.5 seconds in a walk animation. Show location and rotation.

## 2.02 - Get Bone Transform at Frame
Get the transform of the "head" bone at frame 15. Show the result in both local and global space.

## 2.03 - Get Full Pose at Time
Extract the full skeleton pose at time 0.25 seconds. List all bone names and their positions.

## 2.04 - Get Full Pose at Frame
Get the skeleton pose at frame 0 (first frame) and the last frame (use frame count). Compare the root
bone positions.

## 2.05 - Track Bone Movement Over Time
Track the "foot_l" bone position at 10 evenly spaced times throughout an animation. Show the Z position
at each sample.

## 2.06 - Get Root Motion at Time
Get the root motion transform at time 0.5 seconds. Show the accumulated displacement.

## 2.07 - Get Total Root Motion
Get the total root motion for the entire animation. Show forward (X), lateral (Y), vertical (Z) displacement.

## 2.08 - Compare Bone Positions Between Frames
Compare the "hand_r" bone position between frame 0 and frame 30. Calculate the distance traveled.

## 2.09 - Find Highest Point of Bone
Find the frame where the "foot_l" bone reaches its highest Z position during a jump or run animation.

## 2.10 - List Animated Bones
List all animated bones in an animation sequence. Show the first 10 bone names.

---

# Section 3 — Notifies Management

**IMPORTANT NOTES FOR TESTERS:**
- Use `/Script/Engine.AnimNotify` with a custom name for instant notifies that display custom names.
- State notifies require a CONCRETE subclass, NOT the abstract `/Script/Engine.AnimNotifyState` base class.
- `set_notify_color` takes a `LinearColor` object, not separate RGBA values.

## 3.01 - List Existing Notifies
List all notifies in an animation sequence. Show: Notify name, Trigger time, Whether it's a state notify,
Duration (if state).

## 3.02 - Add Instant Notify
Add a footstep sound notify at 0.25 seconds with the name "Footstep_Left". Use `/Script/Engine.AnimNotify`
as the class so the custom name displays in the editor.

## 3.03 - Add Notify State
**NOTE:** Requires a concrete AnimNotifyState subclass — the base `/Script/Engine.AnimNotifyState` is
abstract and cannot be instantiated. If the project has `AnimNotifyState_Trail` or similar, use it;
otherwise skip or create a custom notify state class first. Example: add a trail notify state using
`/Script/Engine.AnimNotifyState_Trail` starting at 0.1s lasting 0.3s.

## 3.04 - Add Multiple Notifies
Add footstep notifies at: 0.0s (left foot), 0.5s (right foot), 1.0s (left foot).

## 3.05 - Remove Notify
Remove the first notify from an animation sequence. Verify by listing notifies again.

## 3.06 - Change Notify Time
Move an existing notify from its current time to 0.75 seconds.

## 3.07 - Change Notify Duration
Change the duration of a state notify to 0.5 seconds.

## 3.08 - Change Notify Track
Move a notify to track index 1 (second track row in editor).

## 3.09 - Get Notify Info by Index
Get detailed information about notify at index 0. Show all available properties.

## 3.10 - Add and Configure Complete Notify
Add a notify, then: (1) set trigger time to 0.33s, (2) set track index to 2, (3) verify by getting notify info.

## 3.11 - Add Notify Track
Add a new notify track named "Footsteps". Verify the track was created.

## 3.12 - Add Notify with Custom Name to Specific Track
Add a "LeftFoot" notify at 0.0s on track 0, then a "RightFoot" notify at 0.15s on track 1. List notifies
to verify both names display correctly.

## 3.13 - Verify Notify Name Display
Add a notify using the base AnimNotify class with the name "TestCustomName". Open the animation editor
and verify the notify displays "TestCustomName" in the timeline, not "AnimNotify".

## 3.14 - Add Multiple Named Notifies to Different Tracks
Set up a walk cycle with proper track organization: (1) add track for left foot events, (2) add track
for right foot events, (3) "LeftFoot" at 0.0s on track 0, (4) "RightFoot" at 0.5s on track 1,
(5) "LeftFoot" at 1.0s on track 0, (6) list all notifies to verify names and track assignments.

## 3.15 - Rename Notify by Remove and Re-Add
Remove an existing notify and re-add it with a different name at the same time position. Verify the new name.

## 3.16 - Rename Notify Directly
Use set_notify_name to rename an existing notify to "RenamedNotify". Verify by getting notify info.

## 3.17 - Set Notify Color
Set a notify's editor display color to bright red (R=1, G=0, B=0, A=1). Verify the color was applied.

## 3.18 - Set Notify Trigger Chance
Set a notify's trigger chance to 50% (0.5) so it randomly triggers half the time. Verify by getting notify info.

## 3.19 - Set Notify Weight Threshold
Set a notify's trigger weight threshold to 0.75 (only fires when blended at ≥75% weight). Verify the change.

## 3.20 - Disable Server Triggering
Disable a notify from triggering on dedicated servers (set trigger_on_server to False). Verify the change.

## 3.21 - Disable Follower Triggering
Disable a notify from triggering on sync group followers (set trigger_on_follower to False). Verify the change.

## 3.22 - Set LOD Filter
Set a notify to only trigger on LOD levels 0-2 (filter type "LOD", filter LOD 2). Verify both filter_type
and filter_lod changed.

## 3.23 - Configure Complete Notify Behavior
Add a new notify and configure ALL behavior properties: (1) name "ConfiguredNotify", (2) color green
(0,1,0,1), (3) trigger chance 80%, (4) weight threshold 0.25, (5) disable server triggering, (6) LOD
filter level 1, (7) get notify info and verify ALL properties.

## 3.24 - List Notify Tracks
List all notify tracks in the animation. Show the track count and any track names.

## 3.25 - Remove Notify Track
Remove a notify track from the animation. Verify the track count decreased.

## 3.26 - Get Notify Track Count
Get the total number of notify tracks in an animation sequence.

## 3.27 - Full Notify Info Dump
Get complete info for a notify showing ALL available properties: notify_name, notify_class, trigger_time,
duration, is_state, track_index, notify_color, trigger_chance, trigger_on_server, trigger_on_follower,
trigger_weight_threshold, notify_filter_type, notify_filter_lod.

## 3.28 - Reset Notify to Defaults
After modifying a notify's behavior, reset to defaults: trigger_chance=1.0, trigger_on_server=True,
trigger_on_follower=True, trigger_weight_threshold=0.0, filter_type="AlwaysTrigger". Verify all returned to defaults.

---

# Section 4 — Curves Management

## 4.01 - List Existing Curves
List all curves in an animation sequence. Show: Curve name, Curve type, Key count, Whether it drives a
morph target.

## 4.02 - Get Curve Info
Get detailed information about a specific curve by name.

## 4.03 - Get Curve Value at Time
Get the value of a curve at time 0.5 seconds.

## 4.04 - Get Curve Keyframes
Get all keyframes for a specific curve. Show time, value, and interpolation mode for each.

## 4.05 - Add New Curve
Add a new float curve named "BlendWeight" to an animation.

## 4.06 - Add Curve Key
Add a key to the "BlendWeight" curve at time 0.5 with value 1.0.

## 4.07 - Set Multiple Curve Keys
Set the "BlendWeight" curve with keys: (0.0, 0.0), (0.25, 0.5), (0.5, 1.0), (0.75, 0.5), (1.0, 0.0).

## 4.08 - Remove Curve
Remove the "BlendWeight" curve from the animation. Verify by listing curves.

## 4.09 - Create Morph Target Curve
Add a curve named "Smile" that drives a morph target. Set it to peak at the midpoint of the animation.

## 4.10 - Evaluate Curve at Multiple Points
Sample a curve at 10 evenly spaced points and plot the values (text output showing the curve shape).

---

# Section 5 — Sync Markers and Root Motion

## 5.01 - List Sync Markers
List all sync markers in an animation. Show marker name and time.

## 5.02 - Add Sync Marker
Add a "LeftFoot" sync marker at time 0.0 seconds.

## 5.03 - Add Multiple Sync Markers
Add sync markers for a walk cycle: "LeftFoot" at 0.0s, "RightFoot" at 0.5s, "LeftFoot" at 1.0s.

## 5.04 - Remove Sync Marker
Remove the "RightFoot" marker at time 0.5 seconds.

## 5.05 - Change Sync Marker Time
Move a sync marker from its current position to 0.25 seconds.

## 5.06 - Check Root Motion Status
Check if root motion is enabled on an animation. Report the status.

## 5.07 - Enable Root Motion
Enable root motion on an animation that currently has it disabled.

## 5.08 - Configure Root Motion Lock
Set the root motion root lock to "AnimFirstFrame" for consistent root position.

## 5.09 - Enable Force Root Lock
Enable force root lock on an animation.

## 5.10 - Full Root Motion Configuration
Configure an animation for root motion: (1) enable root motion, (2) set root lock to AnimFirstFrame,
(3) enable force root lock, (4) get total root motion displacement to verify.

---

# Section 6 — Additive Animation and Editor

## 6.01 - Get Additive Type
Get the additive animation type for an animation. Report None, LocalSpace, or MeshSpace.

## 6.02 - Set Additive Type
Set an animation to be additive with LocalSpace type.

## 6.03 - Get Additive Base Pose
Get the base pose animation path for an additive animation.

## 6.04 - Set Additive Base Pose
Set another animation as the base pose for an additive animation.

## 6.05 - Get Compression Info
Get compression info for an animation: Raw size, Compressed size, Compression ratio, Compression scheme name.

## 6.06 - Compress Animation
Trigger recompression of an animation.

## 6.07 - Export Animation to JSON
Export an animation's metadata and structure to JSON format.

## 6.08 - Get Source Files
Get the source file paths (FBX, etc.) associated with an animation.

## 6.09 - Open Animation Editor
Open an animation sequence in the Animation Editor.

## 6.10 - Full Animation Inspection
Complete inspection: (1) open in editor, (2) full animation info, (3) all bone tracks, (4) all curves,
(5) all notifies, (6) all sync markers, (7) export to JSON.

## 6.11 - Set Rate Scale
Change the playback rate scale of an animation to 1.5 (50% faster).

## 6.12 - Configure Animation for Combat
Set up an animation for combat: (1) add hit notifies at impact frames, (2) add trail notify state for
weapon swing, (3) configure root motion if it has movement, (4) set appropriate rate scale.

---

# Section 7 — Animation Creation

Execute sequentially. Creation uses the UEFN Mannequin skeleton; test assets go in /Game/Tests/Animations/.

## 7.01 - Create Animation from Reference Pose
Create an animation sequence from the UEFN Mannequin skeleton's reference pose. Name it
"AS_TestReferencePose", save in /Game/Tests/Animations/, 1 second long.
**Expected:** asset at `/Game/Tests/Animations/AS_TestReferencePose`, duration 1.0s, saved/visible in Content Browser.

## 7.02 - Create Single-Bone Animation
Create an animation for the UEFN Mannequin skeleton that animates only the spine_01 bone: 0 units at
0.0s → 10 units up (Z) at 0.5s → back to 0 at 1.0s. Name it "AS_TestSpineBounce", save in
/Game/Tests/Animations/, 30 FPS.
**Expected:** spine_01 bone track, 3 keyframes (0.0/0.5/1.0), Z 0→10→0, 30 FPS, 1s duration.

## 7.03 - Create Multi-Bone Rotation Animation
Create an animation for the UEFN Mannequin skeleton animating three bones: spine_01 rotates 45° Yaw over
1s; clavicle_l rotates 30° Pitch; clavicle_r rotates -30° Pitch. Name it "AS_TestMultiBone", save in
/Game/Tests/Animations/, 30 FPS.
**Expected:** 3 bone tracks each with rotation keyframes, 1.0s, 30 FPS.

## 7.04 - Create Animation with 60 FPS
Create a 60 FPS animation from the UEFN Mannequin reference pose. Name it "AS_TestHighFPS", save in
/Game/Tests/Animations/, 0.5 seconds long.
**Expected:** 60 FPS frame rate, 0.5s duration.

## 7.05 - Create Wave Animation
Create a wave-hello animation for the UEFN Mannequin: right arm raises, hand waves side to side 3 times,
arm lowers. Save as "AS_TestWave" in /Game/Tests/Animations/, 2 seconds at 30 FPS.
**Expected:** upperarm_r/lowerarm_r/hand_r tracks; raise (0–0.3s), wave ×3 (0.3–1.5s), lower (1.5–2.0s); 2.0s.

## 7.06 - Invalid Bone Name Handling
Try to create an animation for the UEFN Mannequin skeleton that animates a bone named
"nonexistent_bone_xyz". This should handle the error gracefully.
**Expected:** warning logged about bone not found; no crash or unhandled exception.

## 7.07 - Get Animation Info
Get the detailed info for the AS_TestMultiBone animation, including duration, frame rate, and animated bones.
**Expected:** shows skeleton path, duration, frame rate, bone count.

## 7.08 - Cleanup
Delete all test animations in /Game/Tests/Animations/ that start with "AS_Test".
**Expected:** all test animations removed.
