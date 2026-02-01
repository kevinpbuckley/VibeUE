# Additive Animation and Editor Tests

Test prompts for additive animation configuration and editor operations.

---

## 01 - Get Additive Type

Get the additive animation type for an animation. Report whether it's None, LocalSpace, or MeshSpace.

---

## 02 - Set Additive Type

Set an animation to be additive with LocalSpace type.

---

## 03 - Get Additive Base Pose

Get the base pose animation path for an additive animation.

---

## 04 - Set Additive Base Pose

Set another animation as the base pose for an additive animation.

---

## 05 - Get Compression Info

Get compression information for an animation:
- Raw size
- Compressed size
- Compression ratio
- Compression scheme name

---

## 06 - Compress Animation

Trigger recompression of an animation.

---

## 07 - Export Animation to JSON

Export an animation's metadata and structure to JSON format.

---

## 08 - Get Source Files

Get the source file paths (FBX, etc.) associated with an animation.

---

## 09 - Open Animation Editor

Open an animation sequence in the Animation Editor.

---

## 10 - Full Animation Inspection

Perform a complete inspection of an animation:
1. Open it in the editor
2. Get full animation info
3. List all bone tracks
4. List all curves
5. List all notifies
6. List all sync markers
7. Export to JSON

---

## 11 - Set Rate Scale

Change the playback rate scale of an animation to 1.5 (50% faster).

---

## 12 - Configure Animation for Combat

Set up an animation for use in combat:
1. Add hit notifies at impact frames
2. Add trail notify state for weapon swing
3. Configure root motion if it has movement
4. Set appropriate rate scale
