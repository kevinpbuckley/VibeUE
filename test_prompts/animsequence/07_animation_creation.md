# Animation Creation Test Prompts

Test prompts for AnimSequenceService creation methods. Execute sequentially.

---

## Test 1: Create Animation from Reference Pose

**Prompt:**
```
Create an animation sequence from the UEFN Mannequin skeleton's reference pose. Name it "AS_TestReferencePose" and save it in /Game/Tests/Animations/. Make it 1 second long.
```

**Expected Result:**
- Animation created at `/Game/Tests/Animations/AS_TestReferencePose`
- Duration: 1.0 seconds
- Asset saved and appears in Content Browser

---

## Test 2: Create Single-Bone Animation

**Prompt:**
```
Create an animation for the UEFN Mannequin skeleton that animates only the spine_01 bone. Move it from 0 units at time 0.0 to 10 units up (Z-axis) at time 0.5, then back to 0 at time 1.0. Name it "AS_TestSpineBounce" and save in /Game/Tests/Animations/. Use 30 FPS.
```

**Expected Result:**
- Animation created with spine_01 bone track
- 3 keyframes at times: 0.0, 0.5, 1.0
- Z-axis translation: 0 → 10 → 0
- 30 FPS, 1 second duration

---

## Test 3: Create Multi-Bone Rotation Animation

**Prompt:**
```
Create an animation for the UEFN Mannequin skeleton that animates three bones:
- spine_01: rotates 45 degrees on Yaw over 1 second
- clavicle_l: rotates 30 degrees on Pitch
- clavicle_r: rotates -30 degrees on Pitch

Name it "AS_TestMultiBone" and save in /Game/Tests/Animations/. Use 30 FPS.
```

**Expected Result:**
- Animation with 3 bone tracks
- Each bone has rotation keyframes
- Duration: 1.0 seconds, 30 FPS

---

## Test 4: Create Animation with 60 FPS

**Prompt:**
```
Create a 60 FPS animation from the UEFN Mannequin skeleton's reference pose. Name it "AS_TestHighFPS" and save in /Game/Tests/Animations/. Make it 0.5 seconds long.
```

**Expected Result:**
- Animation with 60 FPS frame rate
- 0.5 second duration

---

## Test 5: Create Wave Animation

**Prompt:**
```
Create a wave hello animation for the UEFN Mannequin skeleton. The right arm should raise up, then the hand should wave side to side 3 times, then the arm should lower back down. Save it as "AS_TestWave" in /Game/Tests/Animations/. Make it 2 seconds at 30 FPS.
```

**Expected Result:**
- Animation with upperarm_r, lowerarm_r, hand_r bone tracks
- Arm raises (0-0.3s), waves 3 times (0.3-1.5s), lowers (1.5-2.0s)
- Duration: 2.0 seconds

---

## Test 6: Invalid Bone Name Handling

**Prompt:**
```
Try to create an animation for the UEFN Mannequin skeleton that animates a bone named "nonexistent_bone_xyz". This should handle the error gracefully.
```

**Expected Result:**
- Warning logged about bone not found
- No crash or unhandled exception

---

## Test 7: Get Animation Info

**Prompt:**
```
Get the detailed info for the AS_TestMultiBone animation, including duration, frame rate, and animated bones.
```

**Expected Result:**
- Shows skeleton path, duration, frame rate, bone count

---

## Cleanup

**Prompt:**
```
Delete all test animations in /Game/Tests/Animations/ that start with "AS_Test".
```

**Expected Result:**
- All test animations removed
