# Bone Tracks and Pose Extraction Tests

Test prompts for extracting bone animation data and poses from animation sequences.

---

## 01 - Get Bone Transform at Time

Get the transform of the "hand_r" bone at time 0.5 seconds in a walk animation. Show location and rotation.

---

## 02 - Get Bone Transform at Frame

Get the transform of the "head" bone at frame 15. Show the result in both local and global space.

---

## 03 - Get Full Pose at Time

Extract the full skeleton pose at time 0.25 seconds. List all bone names and their positions.

---

## 04 - Get Full Pose at Frame

Get the skeleton pose at frame 0 (first frame) and frame -1 (last frame concept - use frame count). Compare the root bone positions.

---

## 05 - Track Bone Movement Over Time

Track the "foot_l" bone position at 10 evenly spaced times throughout an animation. Show the Z position at each sample.

---

## 06 - Get Root Motion at Time

Get the root motion transform at time 0.5 seconds. Show the accumulated displacement.

---

## 07 - Get Total Root Motion

Get the total root motion for the entire animation. Show:
- Forward displacement (X)
- Lateral displacement (Y)
- Vertical displacement (Z)

---

## 08 - Compare Bone Positions Between Frames

Compare the "hand_r" bone position between frame 0 and frame 30. Calculate the distance traveled.

---

## 09 - Find Highest Point of Bone

Find the frame where the "foot_l" bone reaches its highest Z position during a jump or run animation.

---

## 10 - List Animated Bones

List all animated bones in an animation sequence. Show the first 10 bone names.
