# Animation Montage Tests

Consolidated test prompts for AnimMontageService functionality.

---

## Discovery

List all animation montages in this project. Show names, durations, and section counts.

---

Get detailed info about any existing montage - show skeleton, duration, blend settings, sections, slot tracks.

---

Find all montages using the UE5 Mannequin skeleton.

---

## Read Properties (Python)

Get blend settings for an existing montage - blend in/out times, trigger time, blend options.

---

Show all sections in an existing montage with start time, end time, and next section links.

---

List all slot tracks in an existing montage with slot names and segment counts.

---

For an existing montage, list all animation segments per track - show animation path, start time, duration, play rate.

---

## Write Properties (Python)

Set blend in time to 0.15s for an existing montage. Verify the change.

---

Set blend out time to 0.25s and trigger time to -0.1s for an existing montage.

---

Toggle root motion on/off for an existing montage.

---

Set playback rate scale to 1.5x for an existing montage.

---

## Sections (Requires C++)

Add a section called "Attack" at time 0.5s to an existing montage.

---

Remove the section named "Recovery" from an existing montage.

---

Rename section "Attack1" to "LightAttack" in an existing montage.

---

Move the "Attack" section to start at 0.3s instead of current position.

---

Create sections: "WindUp" at 0.0s, "Attack" at 0.2s, "Impact" at 0.5s, "Recovery" at 0.7s.

---

## Section Linking (Requires C++)

Link sections in sequence: WindUp -> Attack -> Recovery.

---

Make the "Idle" section loop forever until interrupted.

---

Remove the link from "Attack" section so it ends the montage.

---

Show the complete flow chart of all section links.

---

## Slot Tracks (Requires C++)

Add a new slot track named "UpperBody" to an existing montage.

---

Change slot name from "DefaultSlot" to "FullBody" on track 0.

---

Remove slot track at index 1.

---

## Animation Segments (Requires C++)

Add an idle animation as segment 2 to track 0 at time 2.0s.

---

Move second segment in track 0 to start at 0.8s.

---

Set segment 0 to play at 1.5x speed and segment 1 at 0.75x speed.

---

Make segment 0 loop 3 times within its allocated duration.

---

Configure segment 0 to play from 0.5s to 2.0s of the source animation.

---

## End-to-End: Multi-Animation Montage (3 Animations, 3 Sections)

Create a montage with 3 DIFFERENT animations, each in its own section:

1. Create an empty montage using the SK_UEFN_Mannequin skeleton
2. Find 3 different animation sequences (e.g., different vault/mantle/climb animations)
3. Add them as sequential segments in Track 0:
   - Animation 1 at time 0.0s
   - Animation 2 at time = Animation 1 duration
   - Animation 3 at time = Animation 1 + Animation 2 duration
4. Create 3 sections matching the animation timing:
   - "Section1" at 0.0s
   - "Section2" at Animation 1 duration
   - "Section3" at Animation 1 + Animation 2 duration
5. Link sections: Section1 -> Section2 -> Section3
6. Set blend in 0.15s, blend out 0.2s
7. Save as AM_MultiAnimation in /Game/Tests/Montages/
8. Use RefreshMontageEditor to ensure UI shows all animations
9. Open and visually verify all 3 animations are visible in the timeline

IMPORTANT: After adding segments, use refresh_montage_editor() to force the UI to update!

---

## End-to-End: Layered Upper Body

Create upper body reload montage:
1. Use any reload/upper body animation
2. Slot track set to "UpperBody"
3. Blend in 0.1s, blend out 0.15s
4. Root motion disabled
Save as AM_Reload_UpperBody in /Game/Tests/Montages/

---

## Summary

Verify:
1. Discovery finds montages, shows info
2. Read properties work via Python (blend, sections, slots, segments)
3. Write properties work via Python (blend times, root motion, rate scale)
4. Section operations work (add, remove, rename, move) - C++ required
5. Section linking works (sequence, loop, clear) - C++ required
6. Slot track operations work (add, rename, remove) - C++ required
7. Segment operations work (add, timing, rate, loop) - C++ required
8. Multi-animation montage creates visible animations in each section
9. Layered montage workflow sets up upper body slot
