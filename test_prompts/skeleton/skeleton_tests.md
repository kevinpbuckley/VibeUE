# Skeleton Service Test Prompts

Run through these prompts sequentially to verify SkeletonService functionality.
Use the UEFN_Mannequin skeleton/mesh for testing as it's available in the project.

---

## Discovery Tests

List all skeletons in the Characters folder.

---

List all skeletal meshes in the Characters folder.

---

Get detailed info about the UEFN_Mannequin skeleton.

---

Get info about the UEFN_Mannequin skeletal mesh including LOD count and socket count.

---

## Bone Hierarchy Tests

List all bones in the UEFN_Mannequin mesh and show the hierarchy.

---

Get detailed info about the "hand_r" bone including its parent and children.

---

Find the root bone of the UEFN_Mannequin skeleton.

---

Get all children of the "spine_01" bone recursively.

---

Find all bones with "hand" in the name.

---

Get the transform of the "head" bone in component space.

---

## Socket Tests

List all sockets on the UEFN_Mannequin mesh.

---

Add a socket called "Weapon_Test" to the "hand_r" bone with a 10cm forward offset and 90 degree rotation.

---

Get info about the "Weapon_Test" socket we just created.

---

Update the "Weapon_Test" socket position to be 15cm forward instead of 10cm.

---

Rename the "Weapon_Test" socket to "Weapon_Right".

---

Move the "Weapon_Right" socket to be attached to the "lowerarm_r" bone instead.

---

Remove the "Weapon_Right" socket.

---

## Retargeting Tests

Get the list of compatible skeletons for the UEFN_Mannequin skeleton.

---

What is the retargeting mode for the "pelvis" bone?

---

Set the "pelvis" bone to use Skeleton retargeting mode.

---

Set the "root" bone to use Skeleton retargeting mode as well.

---

## Curve Metadata Tests

List all curve metadata in the UEFN_Mannequin skeleton.

---

Add a curve called "TestMorphCurve" to the skeleton.

---

Set "TestMorphCurve" to be a morph target curve.

---

Add another curve called "TestMaterialCurve" and set it as a material curve.

---

Rename "TestMorphCurve" to "FacialExpression".

---

Remove the "TestMaterialCurve" curve.

---

## Blend Profile Tests

List all blend profiles in the UEFN_Mannequin skeleton.

---

Create a new blend profile called "TestUpperBody".

---

Set the blend scale for "pelvis" to 0.0 in the "TestUpperBody" profile.

---

Set the blend scale for "spine_03" to 1.0 in the "TestUpperBody" profile.

---

Get the details of the "TestUpperBody" blend profile.

---

## Editor Navigation Tests

Open the UEFN_Mannequin skeleton in the skeleton editor.

---

Open the UEFN_Mannequin skeletal mesh in the mesh editor.

---

## Property Tests

List all morph targets on the UEFN_Mannequin mesh.

---

Get the skeletal mesh info to see the physics asset and post-process AnimBP.

---

## Cleanup

Save any changes we made to the UEFN_Mannequin skeleton.

---

## Summary

Summarize all the skeleton operations we tested and their results.


---

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


---

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


---

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


---

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

