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
