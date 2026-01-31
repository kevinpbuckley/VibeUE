# Bone Modification Tests

These tests verify the bone add/remove/rename/transform functionality using SkeletonModifier.
⚠️ CAUTION: These tests modify the skeleton hierarchy. Use a test mesh or backup first.

**Note**: The UEFN_Mannequin skeleton already has twist bones (upperarm_twist_01_l/r), so we use different names for testing.

---

## Setup

Create a copy of the UEFN_Mannequin skeletal mesh for testing bone modifications. Call it "SKM_BoneTest" in the MCP_Test folder. Delete it if it already exists.

---

## Add Bones

Add a twist bone called "test_twist_01_l" as a child of "upperarm_l" positioned 15cm along the bone (since upperarm_twist_01_l already exists).

---

Add another twist bone "test_twist_01_r" as a child of "upperarm_r" at the same relative position.

---

Commit the bone changes to apply them.

---

Verify the twist bones were added by listing bones with "test_twist" in the name.

---

## Rename Bones

Rename "test_twist_01_l" to "renamed_twist_l".

---

Commit the changes.

---

Verify the rename worked by getting info about "renamed_twist_l".

---

## Reparent Bones (⚠️ KNOWN LIMITATION)

> **WARNING**: Reparenting bones causes the SkeletonModifier's commit to fail with hierarchy mismatch errors.
> This is a limitation of Unreal's SkeletonModifier - it validates that the modified skeleton hierarchy
> matches the original, which conflicts with reparenting operations.
>
> Skip this section in automated testing until Epic fixes this in a future UE version.

Add a new bone called "extra_bone" as a child of "spine_03".

---

Commit and verify the bone was added.

---

Now reparent "extra_bone" to be a child of "spine_02" instead.

---

Commit and verify the parent changed.

---

## Remove Bones

Remove the "extra_bone" we created.

---

Commit the changes.

---

Verify "extra_bone" is no longer in the skeleton.

---

## Transform Operations

Get the current transform of "renamed_twist_l".

---

Modify the transform of "renamed_twist_l" to scale it by 1.1.

---

Commit the changes.

---

## Cleanup

Remove the twist bones we added (renamed_twist_l and test_twist_01_r).

---

Commit the changes.

---

Save the test mesh.

---

Delete the test mesh if desired.
