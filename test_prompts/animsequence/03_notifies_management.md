# Animation Notifies Management Tests

Test prompts for adding, modifying, and removing animation notifies.

**IMPORTANT NOTES FOR TESTERS:**
- Use `/Script/Engine.AnimNotify` with a custom name for instant notifies that display custom names
- State notifies require a CONCRETE subclass, NOT the abstract `/Script/Engine.AnimNotifyState` base class
- `set_notify_color` takes a `LinearColor` object, not separate RGBA values

---

## 01 - List Existing Notifies

List all notifies in an animation sequence. Show:
- Notify name
- Trigger time
- Whether it's a state notify
- Duration (if state)

---

## 02 - Add Instant Notify

Add a footstep sound notify at 0.25 seconds with the name "Footstep_Left".
Use `/Script/Engine.AnimNotify` as the class so the custom name displays in the editor.

---

## 03 - Add Notify State

**NOTE:** This test requires a concrete AnimNotifyState subclass. The base class `/Script/Engine.AnimNotifyState` is abstract and cannot be instantiated.

If the project has `AnimNotifyState_Trail` or similar, use that. Otherwise, skip this test or create a custom notify state class first.

Example with a hypothetical concrete class:
Add a trail notify state using `/Script/Engine.AnimNotifyState_Trail` that starts at 0.1 seconds and lasts for 0.3 seconds.

---

## 04 - Add Multiple Notifies

Add footstep notifies at:
- 0.0 seconds (left foot)
- 0.5 seconds (right foot)
- 1.0 seconds (left foot)

---

## 05 - Remove Notify

Remove the first notify from an animation sequence. Verify it was removed by listing notifies again.

---

## 06 - Change Notify Time

Move an existing notify from its current time to 0.75 seconds.

---

## 07 - Change Notify Duration

Change the duration of a state notify to 0.5 seconds.

---

## 08 - Change Notify Track

Move a notify to track index 1 (second track row in editor).

---

## 09 - Get Notify Info by Index

Get detailed information about notify at index 0. Show all available properties.

---

## 10 - Add and Configure Complete Notify

Add a notify, then:
1. Set its trigger time to 0.33 seconds
2. Set its track index to 2
3. Verify the changes by getting notify info
---

## 11 - Add Notify Track

Add a new notify track named "Footsteps". Verify the track was created.

---

## 12 - Add Notify with Custom Name to Specific Track

Add a "LeftFoot" notify at 0.0 seconds on track 0, then add a "RightFoot" notify at 0.15 seconds on track 1. List notifies to verify both names display correctly.

---

## 13 - Verify Notify Name Display

Add a notify using the base AnimNotify class with the name "TestCustomName". Open the animation editor and verify the notify displays "TestCustomName" in the timeline, not "AnimNotify".

---

## 14 - Add Multiple Named Notifies to Different Tracks

Set up a walk cycle with proper track organization:
1. Add track for left foot events
2. Add track for right foot events  
3. Add "LeftFoot" at 0.0s on track 0
4. Add "RightFoot" at 0.5s on track 1
5. Add "LeftFoot" at 1.0s on track 0
6. List all notifies to verify names and track assignments

---

## 15 - Rename Notify by Remove and Re-Add

Remove an existing notify and re-add it with a different name at the same time position. Verify the new name is applied correctly.

---

## 16 - Rename Notify Directly

Use set_notify_name to rename an existing notify from its current name to "RenamedNotify". Verify the name changed by getting notify info.

---

## 17 - Set Notify Color

Set a notify's editor display color to bright red (R=1, G=0, B=0, A=1). Verify the color was applied.

---

## 18 - Set Notify Trigger Chance

Set a notify's trigger chance to 50% (0.5). This makes the notify randomly trigger only half the time. Verify by getting notify info.

---

## 19 - Set Notify Weight Threshold

Set a notify's trigger weight threshold to 0.75. This means the notify only fires when the animation is blended at 75% or higher weight. Verify the change.

---

## 20 - Disable Server Triggering

Disable a notify from triggering on dedicated servers (set trigger_on_server to False). This is useful for visual-only notifies. Verify the change.

---

## 21 - Disable Follower Triggering

Disable a notify from triggering on sync group followers (set trigger_on_follower to False). Verify the change.

---

## 22 - Set LOD Filter

Set a notify to only trigger on LOD levels 0-2 by setting filter type to "LOD" and filter LOD to 2. Verify both filter_type and filter_lod changed.

---

## 23 - Configure Complete Notify Behavior

Add a new notify and configure ALL behavior properties:
1. Set name to "ConfiguredNotify"
2. Set color to green (0, 1, 0, 1)
3. Set trigger chance to 80%
4. Set weight threshold to 0.25
5. Disable server triggering
6. Set LOD filter to level 1
7. Get notify info and verify ALL properties are set correctly

---

## 24 - List Notify Tracks

List all notify tracks in the animation. Show the track count and any track names.

---

## 25 - Remove Notify Track

Remove a notify track from the animation. Verify the track count decreased.

---

## 26 - Get Notify Track Count

Get the total number of notify tracks in an animation sequence.

---

## 27 - Full Notify Info Dump

Get complete info for a notify showing ALL available properties:
- notify_name
- notify_class
- trigger_time
- duration
- is_state
- track_index
- notify_color
- trigger_chance
- trigger_on_server
- trigger_on_follower
- trigger_weight_threshold
- notify_filter_type
- notify_filter_lod

---

## 28 - Reset Notify to Defaults

After modifying a notify's behavior settings, reset them to defaults:
- trigger_chance = 1.0 (always trigger)
- trigger_on_server = True
- trigger_on_follower = True
- trigger_weight_threshold = 0.0
- filter_type = "AlwaysTrigger"

Verify all properties returned to defaults.