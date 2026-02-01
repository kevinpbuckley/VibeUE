# Animation Sequence Discovery Tests

Test prompts for discovering and querying animation sequence assets using AnimSequenceService.

Reference: Use any animation sequences in the project, or create test animations.

---

## 01 - List All Animation Sequences

List all animation sequences in /Game and show their names and durations.

---

## 02 - Get Animation Sequence Info

Get detailed information about a specific animation sequence. Show:
- Duration
- Frame rate
- Frame count
- Bone track count
- Curve count
- Notify count
- Root motion enabled status

---

## 03 - Find Animations for Skeleton

Find all animation sequences that use the SK_UEFN_Mannequin skeleton. List their names and durations.

---

## 04 - Search Animations by Pattern

Search for all animations with "Walk" in the name. List matching animations.

---

## 05 - Search Animations by Pattern (Wildcard)

Search for animations matching "*Run*" pattern. Show results.

---

## 06 - Get Animation Properties

For a specific animation:
1. Get the length in seconds
2. Get the frame rate
3. Get the frame count
4. Get the skeleton path
5. Get the rate scale

---

## 07 - Get Animated Bones

List all bones that have animation data in a specific animation sequence.

---

## 08 - Get Bone Tracks

Get all bone tracks in an animation and show:
- Bone name
- Position key count
- Rotation key count
- Scale key count

---

## 09 - Compare Two Animations

Compare two animation sequences:
1. List them both with their properties
2. Show which has more frames
3. Show which has more curves
4. Show which has more notifies

---

## 10 - List Animations with Root Motion

Find all animations that have root motion enabled. List their names and total root motion displacement.
