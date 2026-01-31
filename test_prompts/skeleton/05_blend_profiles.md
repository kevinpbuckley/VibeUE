# Blend Profile Setup

This test demonstrates creating blend profiles for layered animation blending.

---

## Check Existing Profiles

List all blend profiles in the UEFN_Mannequin skeleton.

---

## Create Upper Body Profile

Create a blend profile called "UpperBody" - this will be used for upper body aiming/shooting while legs play locomotion.

---

## Configure Lower Body (No Blend)

Set these lower body bones to 0.0 blend scale in the UpperBody profile:
- pelvis
- thigh_l
- thigh_r
- calf_l
- calf_r
- foot_l
- foot_r

---

## Configure Spine (Gradual Blend)

Set spine bones with gradual blend:
- spine_01: 0.3
- spine_02: 0.6
- spine_03: 1.0

---

## Configure Upper Body (Full Blend)

Set these to 1.0:
- clavicle_l
- clavicle_r
- upperarm_l
- upperarm_r
- neck_01
- head

---

## Verify Profile

Get the details of the UpperBody blend profile to see the bone scales.

---

## Create Arm Only Profile

Create another profile called "ArmsOnly" for gestures that only affect the arms.

---

Set spine bones to 0.0 in ArmsOnly profile.

---

Set clavicle and arm bones to 1.0 in ArmsOnly profile:
- clavicle_l
- clavicle_r
- upperarm_l
- upperarm_r
- lowerarm_l
- lowerarm_r
- hand_l
- hand_r

---

## Create Head Only Profile

Create a "HeadOnly" profile for look-at or dialogue animations.

---

Set only these to 1.0 in HeadOnly:
- neck_01
- head

---

## Final Verification

List all blend profiles to confirm we created three.

---

Get details of the ArmsOnly profile.

---

Get details of the HeadOnly profile.

---

## Save

Save the skeleton with all blend profile changes.
