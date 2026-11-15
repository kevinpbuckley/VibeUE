---
name: animsequence-retarget-recipe
description: Retarget animation clips between skeletons (for example generated or mocap clips onto a MetaHuman body) from editor Python with the engine's IK Rig, IK Retargeter and batch retarget, including the legacy FBX importer switch for animation imports, the controller-does-not-dirty save trap, the MetaHuman pelvis fix and bulk bone-key writes.
---

# Retarget recipe: IK Rig + IK Retargeter + batch retarget (UE 5.9)

VibeUE has no batch retarget: `AnimSequenceService.retarget_preview(anim_path, target_skeleton_path)` only
previews a clip on another skeleton and writes nothing, and `SkeletonService.set_bone_retargeting_mode` only
sets a bone's translation retargeting mode. Writing retargeted clips is the engine's IK Retargeter, driven
from `execute_python`.

This recipe is distilled from a script that retargeted generated clips onto six MetaHuman bodies in a 48-hour
UE 5.9 project. **[ran]** marks calls that ran there; **[header]** marks calls read in the 5.9 headers and not
run. Check a **[header]** call with `get_symbol` before relying on it.

Every `import` goes at the top level of the script; each `execute_python` call has a fresh namespace.

## 1. Import the source clips with the classic FBX importer

5.9 routes FBX through Interchange, which **ignores `unreal.FbxImportUI`**: a custom sample rate, the target
skeleton, `import_translation` and the animation length setting are silently dropped. Turn Interchange's FBX
handling off around the import and back on afterwards **[ran]**:

```python
import unreal

def legacy_fbx(on=True):
    # Interchange.FeatureFlags.Import.FBX is the engine cvar UFbxFactory checks; 0 = classic importer.
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX " + ("0" if on else "1"))

def import_anim(fbx, dest, name, skeleton):
    t = unreal.AssetImportTask()
    t.filename, t.destination_path, t.destination_name = fbx, dest, name
    t.automated, t.save, t.replace_existing = True, True, True
    o = unreal.FbxImportUI()
    o.import_mesh, o.import_as_skeletal, o.import_animations = False, True, True
    o.import_materials = o.import_textures = o.create_physics_asset = False
    o.skeleton = skeleton                                    # the source mesh's skeleton
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    d = o.anim_sequence_import_data
    d.set_editor_property("use_default_sample_rate", False)
    d.set_editor_property("custom_sample_rate", 30)
    d.set_editor_property("snap_to_closest_frame_boundary", True)
    d.set_editor_property("remove_redundant_keys", False)
    d.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    t.options = o
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    return list(t.get_editor_property("imported_object_paths"))

legacy_fbx(True)
try:
    paths = import_anim(r"D:/clips/wave.fbx", "/Game/Anim/Source", "A_Wave_Src",
                        unreal.load_asset("/Game/Anim/Source/SKM_Source").get_editor_property("skeleton"))
finally:
    legacy_fbx(False)                                        # put it back on every path
paths
```

For the first import (mesh + skeleton), set `import_mesh = True` and
`mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH` and leave `skeleton` unset. Epic's
`EditorToolset.SkeletalMeshToolset.ImportFile` imports through Interchange; do not use it when the FBX
animation options matter.

## 2. Build the source IK Rig [ran]

The target side usually has a rig already (MetaHuman bodies:
`/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig`). The source needs one whose chain NAMES match the
target rig's chains, so auto-mapping can pair them.

```python
import unreal
EAL = unreal.EditorAssetLibrary
SRC_MESH = "/Game/Anim/Source/SKM_Source"
# (chain name, start bone, end bone): chain names as in the target rig, bones from the source skeleton
CHAINS = [("Root", "root", "root"), ("Spine", "Spine1", "Chest"), ("Neck", "Neck1", "Neck2"),
          ("Head", "Head", "Head"), ("LeftClavicle", "LeftShoulder", "LeftShoulder"),
          ("LeftArm", "LeftArm", "LeftHand"), ("RightClavicle", "RightShoulder", "RightShoulder"),
          ("RightArm", "RightArm", "RightHand"), ("LeftLeg", "LeftLeg", "LeftToeBase"),
          ("RightLeg", "RightLeg", "RightToeBase")]

rig = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "IK_Source", "/Game/Anim/Source", unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
c = unreal.IKRigController.get_controller(rig)
c.set_skeletal_mesh(unreal.load_asset(SRC_MESH))
c.set_retarget_root("Hips")                                  # the source pelvis bone
for name, start, end in CHAINS:
    c.add_retarget_chain(name, start, end, "")               # "" = no IK goal
EAL.save_asset("/Game/Anim/Source/IK_Source")
```

Read the target rig's chain names first (open it, or list them) and copy them exactly. A standard humanoid
source may also take `c.apply_auto_generated_retarget_definition()` **[header]** instead of hand-written chains.

## 3. Build the IK Retargeter, and SAVE it so it survives a restart [ran]

```python
import unreal
EAL = unreal.EditorAssetLibrary
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
RTG = "/Game/Anim/Source/RTG_Source_to_MH"

rtg = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "RTG_Source_to_MH", "/Game/Anim/Source", unreal.IKRetargeter, unreal.IKRetargetFactory())
rc = unreal.IKRetargeterController.get_controller(rtg)
rc.set_ik_rig(S, unreal.load_asset("/Game/Anim/Source/IK_Source"))
rc.set_ik_rig(T, unreal.load_asset("/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig"))
rc.set_preview_mesh(S, unreal.load_asset("/Game/Anim/Source/SKM_Source"))
rc.set_preview_mesh(T, unreal.load_asset("/Game/Characters/Hero/Body/SKM_Hero_BodyMesh"))
rc.add_default_ops()
rc.auto_map_chains(unreal.AutoMapChainType.EXACT, True)      # pair chains by identical name
# Align the reference poses: target to the source's pose, chain by chain.
rc.auto_align_all_bones(T, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
# A source whose rest pose has the hips at the floor (a BVH rest, for one) needs a root offset (cm):
rc.set_root_offset_in_retarget_pose(unreal.Vector(0, 0, 92.0), S)
rtg.modify()                                                 # the controller does NOT dirty the package
EAL.save_asset(RTG, only_if_is_dirty=False)
str(rc.get_root_offset_in_retarget_pose(S))
```

**The retargeter controller does not mark the package dirty.** A plain `save_asset(path)` skips it, and the
pose alignment and root offset are gone after the next editor start: the clip retargets correctly in the session
that built it and wrong in every later one. Call `modify()` and save with `only_if_is_dirty=False`, and
re-check `get_root_offset_in_retarget_pose` before each batch.

## 4. Retarget the clips

The 5.9 call is `RunBatchRetarget` with an inputs struct **[header]** (`DuplicateAndRetarget` is deprecated
since 5.8):

```python
import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
inp = unreal.IKRetargetBatchOperationInputs()
inp.assets_to_retarget = [ar.get_asset_by_object_path("/Game/Anim/Source/A_Wave_Src.A_Wave_Src")]
inp.source_mesh = unreal.load_asset("/Game/Anim/Source/SKM_Source")
inp.target_mesh = unreal.load_asset("/Game/Characters/Hero/Body/SKM_Hero_BodyMesh")
inp.ik_retarget_asset = unreal.load_asset("/Game/Anim/Source/RTG_Source_to_MH")
inp.search, inp.replace = "_Src", "_Hero"                    # output name: A_Wave_Hero
inp.target_path = "/Game/Anim/Body"
inp.include_referenced_assets = False
inp.overwrite_existing_files = True
results = unreal.IKRetargetBatchOperation.run_batch_retarget_with_results(inp)
[(str(r.source_asset.package_name), str(r.retargeted_asset.package_name), str(r.status), r.reason)
 for r in results]
```

`run_batch_retarget(inp)` returns only the new assets; `run_batch_retarget_with_results` says per asset whether
it was retargeted, skipped or failed and why. Both raise a script error, before duplicating anything, when no op
of the retargeter can run or a sequence would come out empty. Save the outputs
(`unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)`).

The jam ran the deprecated form, which still works in 5.9 **[ran]**:
`unreal.IKRetargetBatchOperation.duplicate_and_retarget([asset_data], source_mesh, target_mesh, rtg, search,
replace, prefix, suffix, target_path, use_source_path, include_referenced_assets, overwrite_existing_files)`.

Retarget per character with that character's own body mesh as `target_mesh`: proportions differ, and a clip
retargeted onto one body sits wrong on another.

## 5. The MetaHuman pelvis fix [ran]

After a retarget onto a MetaHuman body, the pelvis can stay at the floor while the hip height moves into
`spine_01` and the thighs, which stretches the hip skinning (worst under dresses and long coats). The fix
re-splits every key so nothing moves in component space: `spine_01` gets the local offset of a reference clip
on the same body, the pelvis takes the rest, the thighs follow.

```python
import unreal
AL = unreal.AnimationLibrary

def fix_pelvis(path, ref_path):
    a, ref = unreal.load_asset(path), unreal.load_asset(ref_path)        # ref: an idle on the same body
    ref_s = AL.get_bone_pose_for_time(ref, "spine_01", 0.0, False)
    length, n = a.get_play_length(), AL.get_num_keys(a)
    kids = ["spine_01", "thigh_l", "thigh_r"]
    out = {b: ([], [], []) for b in ["pelvis"] + kids}
    for k in range(n):
        t = min(length, k * length / (n - 1))
        pelvis = AL.get_bone_pose_for_time(a, "pelvis", t, False)
        cs = {b: AL.get_bone_pose_for_time(a, b, t, False).multiply(pelvis) for b in kids}
        spine = AL.get_bone_pose_for_time(a, "spine_01", t, False)
        new_spine = unreal.Transform(ref_s.translation, spine.rotation.rotator(), spine.scale3d)
        new_pelvis = new_spine.inverse().multiply(cs["spine_01"])
        new = {"pelvis": new_pelvis, "spine_01": new_spine}
        for b in ("thigh_l", "thigh_r"):
            new[b] = cs[b].multiply(new_pelvis.inverse())
        for b, tr in new.items():
            out[b][0].append(tr.translation); out[b][1].append(tr.rotation); out[b][2].append(tr.scale3d)
    c = a.controller                                         # a property, not get_controller()
    c.open_bracket(unreal.Text("fix pelvis"))
    for b, (p, r, s) in out.items():
        c.set_bone_track_keys(b, p, r, s)                    # one call per bone, every key at once
    c.close_bracket()
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return n
```

`AnimationLibrary.get_bone_pose_for_time(anim, bone, time, extract_root_motion)` returns the bone's LOCAL
transform; multiply up the parent chain for component space. The same bulk-write pattern
(`anim.controller` + `open_bracket` + `set_bone_track_keys` per bone + `close_bracket`) is the way to write any
computed animation: one call per bone instead of one key per call.

## 6. Check the result, then look at it

- Read numbers back: pelvis height over the clip, a hand against the head for a hands-to-face take, pelvis
  travel for an entrance. Pick between takes by measurement.
- Capture a few frames and look at them:
  `unreal.AnimSequenceService.capture_animation_sequence(...)` (see `api-reference.md`) renders in a private
  preview scene; or play it in PIE.
- A clip whose root motion carries the character far from the actor origin can vanish mid-shot: set the
  skeletal mesh components' `visibility_based_anim_tick_option` to
  `unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES`.

| Symptom | Cause | Fix |
|---|---|---|
| Import ignores the frame rate, skeleton or length | Interchange handled the FBX | Step 1: `Interchange.FeatureFlags.Import.FBX 0` around the import |
| Retarget right today, wrong after a restart | Retargeter settings never saved | Step 3: `modify()` + `save_asset(..., only_if_is_dirty=False)` |
| Hips stretched, pelvis at the floor | Hip height went into `spine_01` | Step 5: pelvis fix |
| `RunBatchRetarget` raises before doing anything | No retargeter op can run (rigs missing or not matching the meshes), or an empty source clip | Check `set_ik_rig` / `set_preview_mesh` on both sides; drop the empty clip |
