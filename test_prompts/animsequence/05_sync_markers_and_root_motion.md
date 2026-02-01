# Sync Markers and Root Motion Tests

Test prompts for sync markers and root motion configuration.

---

## 01 - List Sync Markers

List all sync markers in an animation. Show marker name and time.

---

## 02 - Add Sync Marker

Add a "LeftFoot" sync marker at time 0.0 seconds.

---

## 03 - Add Multiple Sync Markers

Add sync markers for a walk cycle:
- "LeftFoot" at 0.0s
- "RightFoot" at 0.5s
- "LeftFoot" at 1.0s

---

## 04 - Remove Sync Marker

Remove the "RightFoot" marker at time 0.5 seconds.

---

## 05 - Change Sync Marker Time

Move a sync marker from its current position to 0.25 seconds.

---

## 06 - Check Root Motion Status

Check if root motion is enabled on an animation. Report the status.

---

## 07 - Enable Root Motion

Enable root motion on an animation that currently has it disabled.

---

## 08 - Configure Root Motion Lock

Set the root motion root lock to "AnimFirstFrame" for consistent root position.

---

## 09 - Enable Force Root Lock

Enable force root lock on an animation.

---

## 10 - Full Root Motion Configuration

Configure an animation for root motion:
1. Enable root motion
2. Set root lock to AnimFirstFrame
3. Enable force root lock
4. Get total root motion displacement to verify
