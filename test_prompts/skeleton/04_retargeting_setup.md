# Retargeting and Animation Sharing Setup

This test demonstrates setting up a skeleton for animation retargeting.

---

## Check Current State

Get the current retargeting mode for the root bone.

---

Get the current retargeting mode for the pelvis bone.

---

List any compatible skeletons already configured.

---

## Configure Root and Pelvis

Set "root" bone to Skeleton retargeting mode - this preserves the skeleton's root position.

---

Set "pelvis" bone to Skeleton retargeting mode - this preserves hip height.

---

## Configure IK Bones

Set "ik_foot_root" to Animation retargeting mode.

---

Set "ik_foot_l" to Animation mode.

---

Set "ik_foot_r" to Animation mode.

---

Set "ik_hand_root" to Animation mode.

---

Set "ik_hand_l" to Animation mode.

---

Set "ik_hand_r" to Animation mode.

---

## Verify Configuration

Get info about the pelvis bone and confirm the retargeting mode.

---

Get info about the ik_foot_l bone and confirm Animation mode.

---

## Animation Curves

Add curve metadata for standard locomotion curves:
- Add "Speed" curve
- Add "Direction" curve
- Add "IsMoving" curve

---

These are animation curves, not morph or material curves, so leave their types as default.

---

## Morph Target Curves

Add facial expression curves:
- Add "Smile" and mark it as morph target
- Add "EyesClosed" and mark it as morph target

---

## Material Curves

Add visual effect curves:
- Add "DamageFlash" and mark it as material curve

---

## List All Curves

List all curve metadata to verify our setup.

---

## Save

Save the skeleton with all retargeting and curve changes.
