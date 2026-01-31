# Socket Setup Workflow

This test demonstrates a complete socket setup workflow for a character.

---

## Initial State

List all current sockets on the UEFN_Mannequin mesh.

---

## Weapon Sockets

Add a right hand weapon socket called "Weapon_R" on "hand_r" with:
- 10cm forward offset
- 90 degree Z rotation
- Add to skeleton so it's shared

---

Add a left hand weapon socket called "Weapon_L" on "hand_l" with:
- 10cm forward offset
- -90 degree Z rotation
- Add to skeleton

---

## Equipment Sockets

Add a back holster socket called "Holster_Back" on "spine_03" with:
- 20cm backward offset (negative Y)
- 90 degree yaw
- Add to skeleton

---

Add a hip holster socket called "Holster_Hip_R" on "thigh_r" with:
- 10cm to the right (positive X)
- Add to skeleton

---

## Head Attachments

Add a helmet attachment socket called "Helmet" on "head" with:
- 20cm up offset (positive Z)
- Mesh-specific (not shared)

---

Add a camera mount socket called "Camera_FP" on "head" with:
- 15cm forward, 10cm up
- Mesh-specific for this character

---

## Verify Setup

List all sockets now and confirm they were all added correctly.

---

Get detailed info about the "Weapon_R" socket.

---

## Adjustments

The Weapon_R socket is slightly off. Update its position to be 12cm forward instead of 10cm.

---

The helmet socket should be on the skeleton level. Remove it and re-add with bAddToSkeleton=True.

---

## Save

Save the skeletal mesh to persist all socket changes.

---

## Final Verification

List all sockets one more time to confirm the final configuration.
