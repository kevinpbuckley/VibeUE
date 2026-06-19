---
name: skeleton
display_name: Skeleton & Skeletal Mesh Management
description: Edit skeleton bone transforms/hierarchy, retargeting modes, curve metadata, blend profiles, bone constraints, and learn constraints from animations (VibeUE SkeletonService). Socket CRUD, bone enumeration, physics-asset, and material/LOD inspection are owned by the engine SkeletalMeshTools. Use when the user asks to add/transform bones, set retargeting, manage blend profiles or curves, or set bone constraints. For animation bone edits, also load animation-editing.
vibeue_classes:
  - SkeletonService
unreal_classes:
  - Skeleton
  - SkeletalMesh
  - SkeletonModifier
  - BlendProfile
keywords:
  - skeleton
  - skeletal mesh
  - bone
  - bone transform
  - retarget
  - curve
  - blend profile
  - skeleton profile
  - bone constraint
  - joint limit
  - learned constraints
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "animation-system"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Skeleton & Skeletal Mesh Skill

> **Architecture (read first):** VibeUE extends Unreal's native MCP endpoint. **Socket CRUD, bone
> enumeration, physics-asset assignment, and material/LOD/morph inspection** are owned by the **engine**
> toolset `editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools` (called via `call_tool`). VibeUE's
> `SkeletonService` was trimmed to what the engine doesn't cover: **bone transform/hierarchy editing,
> retargeting config, curve metadata, blend profiles, bone constraints, and learn-from-animations**.
>
> **Loading skills:** skills load through the engine's `AgentSkillToolset` (`ListSkills`/`GetSkills`) —
> there is no `vibeue-skills-manager` tool. Run VibeUE services with `execute_python_code`
> (`unreal.SkeletonService.<method>()`); reach engine tools with `call_tool` (discover with
> `list_toolsets` / `describe_toolset`).
>
> **Related Skills:**
> - **animsequence / animation-editing** - For editing animations with constraint validation (uses skeleton profiles) and keyframes
> - **animation-blueprint** - For AnimBP state machines and navigation
>
> **Use this skill when:** Editing bone transforms/hierarchy, retargeting modes, blend profiles, curve metadata, or bone constraints. For **sockets / bone listing / physics asset / materials / LODs**, use engine `SkeletalMeshTools` (see below).

## Sockets, bone enumeration, inspection → engine SkeletalMeshTools

These moved to the engine. Discover the exact schemas with
`describe_toolset("editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools")` and call via `call_tool`
(the `mesh` argument is a `{"refPath": "<SkeletalMesh asset path>"}` object):

| Task | Engine tool (`SkeletalMeshTools.*`) |
|------|--------------------------------------|
| List sockets | `get_socket_names` |
| Add / remove / rename socket | `add_socket`, `remove_socket`, `rename_socket` |
| Get / set socket transform, get socket bone | `get_socket_transform`, `set_socket_transform`, `get_socket_bone` |
| List bones / parent / children | `get_bone_names`, `get_bone_parent`, `get_bone_children` |
| Material slots / get / set | `get_material_slots`, `get_material`, `set_material` |
| Physics asset get / assign | `get_physics_asset`, `assign_physics_asset` |
| LODs / vertices / sections / bounds | `get_lod_count`, `get_vertex_count`, `get_section_count`, `get_bounds` |
| Morph target names | `get_morph_target_names` |

```python
# Example: add a socket on the hand_r bone, then list sockets
call_tool(toolset_name="editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools",
          tool_name="add_socket",
          arguments={"mesh": {"refPath": "/Game/Characters/SKM_Mannequin"},
                     "socket_name": "Weapon_R", "bone_name": "hand_r"})
call_tool(toolset_name="editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools",
          tool_name="set_socket_transform",
          arguments={"mesh": {"refPath": "/Game/Characters/SKM_Mannequin"},
                     "socket_name": "Weapon_R",
                     "transform": {"location": {"x":10,"y":0,"z":0},
                                   "rotation": {"pitch":0,"yaw":90,"roll":0},
                                   "scale": {"x":1,"y":1,"z":1}}})
```

> Note: the engine `add_socket` always adds the socket to the **skeletal mesh**. VibeUE's old
> `bAddToSkeleton` (skeleton-wide socket) flag is gone — to share a socket across all meshes on a
> skeleton, add it on each mesh, or add it on the skeleton via `unreal.*` directly.

## Critical Rules

### ⚠️ Bone Modification Requires Commit

Bone modifications (add, remove, rename, reparent) use a transaction system. Changes are NOT applied until you call `commit_bone_changes()`:

```python
import unreal

# Make bone changes
unreal.SkeletonService.add_bone("/Game/SKM_Character", "twist_arm_l", "upperarm_l", unreal.Transform())
unreal.SkeletonService.add_bone("/Game/SKM_Character", "twist_arm_r", "upperarm_r", unreal.Transform())

# ⚠️ CRITICAL: Commit changes!
unreal.SkeletonService.commit_bone_changes("/Game/SKM_Character")
```

**Forgetting to commit = changes are lost!**

> ⚠️ **Do NOT reparent bones via SkeletonModifier (known-broken, UE 5.7).** Changing a bone's
> parent and calling `commit_bone_changes()` fails with a hierarchy-mismatch validation error and
> can leave the commit stalled (the call may not return cleanly). Add/remove/rename/transform are
> safe; reparenting is not. If a user needs a different hierarchy, add a new bone at the desired
> parent and remove the old one rather than reparenting in place.

### Sockets are now an engine operation

Socket CRUD is on `SkeletalMeshTools` (see the table at the top). The engine `add_socket` attaches to
the skeletal mesh; set its offset with `set_socket_transform`. There is no `bAddToSkeleton` flag.

### Skeleton vs SkeletalMesh Paths

- **Skeleton** operations (retargeting, curves, blend profiles, constraints) use `SK_` assets → VibeUE `SkeletonService`
- **SkeletalMesh** bone *transform/hierarchy edits* use `SKM_` assets → VibeUE `SkeletonService`
- **SkeletalMesh** sockets / bone listing / inspection use `SKM_` assets → engine `SkeletalMeshTools`

```python
# Skeleton operations (VibeUE) - use SK_ path
unreal.SkeletonService.get_skeleton_info("/Game/Characters/SK_Mannequin")
unreal.SkeletonService.set_bone_retargeting_mode("/Game/Characters/SK_Mannequin", "pelvis", "Skeleton")

# Sockets / bone listing (engine) - use SKM_ path via call_tool, e.g.:
# call_tool("editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools", "get_socket_names",
#           {"mesh": {"refPath": "/Game/Characters/SKM_Mannequin"}})
```

### Retargeting Mode Reference

| Mode | Description | Use For |
|------|-------------|---------|
| `Animation` | Use animation data directly | Most bones (default) |
| `Skeleton` | Use skeleton's reference pose | Root, pelvis, IK targets |
| `AnimationScaled` | Scale animation by skeleton size | Height-scaled characters |
| `OrientAndScale` | Match orientation and scale | Helper/utility bones |

### Skeleton Profiles & Constraints

For animation editing with constraint validation, use the skeleton profile methods:

```python
import unreal

# Create/refresh skeleton profile with hierarchy and constraints
profile = unreal.SkeletonService.create_skeleton_profile("/Game/SK_Mannequin")
print(f"Skeleton has {profile.bone_count} bones")

# Learn constraints from existing animations
constraints = unreal.SkeletonService.learn_from_animations("/Game/SK_Mannequin", 50, 10)
print(f"Learned from {constraints.animation_count} animations")

# Set manual bone constraints (e.g., elbow as hinge)
unreal.SkeletonService.set_bone_constraints(
    "/Game/SK_Mannequin",
    "lowerarm_r",
    unreal.Rotator(0, 0, 0),     # Min rotation
    unreal.Rotator(0, 145, 0),   # Max rotation
    True,  # Is hinge joint
    1      # Pitch axis (Y)
)

# Validate a rotation against constraints
result = unreal.SkeletonService.validate_bone_rotation(
    "/Game/SK_Mannequin",
    "upperarm_r",
    unreal.Rotator(0, 180, 0),  # Test rotation
    True  # Use learned constraints
)
if not result.is_valid:
    print(f"Clamped to: {result.clamped_rotation}")
```

> **See the `animsequence` skill** for complete preview→validate→bake workflow.

## Workflows

### List All Bones (engine SkeletalMeshTools)

Bone enumeration moved to the engine. Use `get_bone_names` (hierarchy order, root first) and walk
parent/children with `get_bone_parent` / `get_bone_children`:

```python
# names = call_tool("editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools",
#                   "get_bone_names", {"mesh": {"refPath": "/Game/Characters/SKM_Mannequin"}})
# for n in names:
#     children = call_tool(... "get_bone_children",
#                          {"mesh": {"refPath": "/Game/Characters/SKM_Mannequin"}, "bone_name": n})
```

For a richer hierarchy with transforms/depth/retarget mode, VibeUE's
`create_skeleton_profile(skeleton_path)` returns `bone_hierarchy` (Array[BoneNodeInfo]) — see below.

### Add Twist Bones for Better Skinning

```python
import unreal

mesh_path = "/Game/Characters/SKM_Character"

# Add forearm twist bones
unreal.SkeletonService.add_bone(
    mesh_path,
    "lowerarm_twist_01_l",
    "lowerarm_l",
    unreal.Transform(location=[12.0, 0.0, 0.0])
)

unreal.SkeletonService.add_bone(
    mesh_path,
    "lowerarm_twist_01_r",
    "lowerarm_r",
    unreal.Transform(location=[12.0, 0.0, 0.0])
)

# Add upperarm twist bones
unreal.SkeletonService.add_bone(
    mesh_path,
    "upperarm_twist_01_l",
    "upperarm_l",
    unreal.Transform(location=[15.0, 0.0, 0.0])
)

unreal.SkeletonService.add_bone(
    mesh_path,
    "upperarm_twist_01_r",
    "upperarm_r",
    unreal.Transform(location=[15.0, 0.0, 0.0])
)

# CRITICAL: Commit the changes
unreal.SkeletonService.commit_bone_changes(mesh_path)
unreal.SkeletonService.save_asset(mesh_path)
```

### Set Up Weapon Sockets (engine SkeletalMeshTools)

Socket CRUD is on the engine toolset. Add the socket, then set its offset transform:

```python
mesh = {"refPath": "/Game/Characters/SKM_Character"}
TS = "editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools"

call_tool(TS, "add_socket", {"mesh": mesh, "socket_name": "Weapon_R", "bone_name": "hand_r"})
call_tool(TS, "set_socket_transform", {"mesh": mesh, "socket_name": "Weapon_R",
          "transform": {"location": {"x":10,"y":0,"z":0},
                        "rotation": {"pitch":0,"yaw":90,"roll":0},
                        "scale": {"x":1,"y":1,"z":1}}})

call_tool(TS, "add_socket", {"mesh": mesh, "socket_name": "Holster_Back", "bone_name": "spine_03"})
call_tool(TS, "set_socket_transform", {"mesh": mesh, "socket_name": "Holster_Back",
          "transform": {"location": {"x":0,"y":-20,"z":0},
                        "rotation": {"pitch":0,"yaw":0,"roll":90},
                        "scale": {"x":1,"y":1,"z":1}}})
```

### Configure Retargeting for Animation Sharing

```python
import unreal

skeleton_path = "/Game/Characters/SK_Character"

# Root and pelvis should use Skeleton mode
for bone in ["root", "pelvis"]:
    unreal.SkeletonService.set_bone_retargeting_mode(skeleton_path, bone, "Skeleton")

# IK bones should use Animation mode
for bone in ["ik_foot_l", "ik_foot_r", "ik_hand_l", "ik_hand_r"]:
    unreal.SkeletonService.set_bone_retargeting_mode(skeleton_path, bone, "Animation")

# Add a compatible skeleton for sharing animations
unreal.SkeletonService.add_compatible_skeleton(
    skeleton_path,
    "/Game/Mannequins/SK_Mannequin"
)

unreal.SkeletonService.save_asset(skeleton_path)
```

### Create Upper Body Blend Profile

```python
import unreal

skeleton_path = "/Game/Characters/SK_Character"

# Create the blend profile
unreal.SkeletonService.create_blend_profile(skeleton_path, "UpperBody")

# Set lower body to 0 (no blend)
lower_body_bones = ["pelvis", "thigh_l", "thigh_r", "calf_l", "calf_r", "foot_l", "foot_r"]
for bone in lower_body_bones:
    unreal.SkeletonService.set_blend_profile_bone(skeleton_path, "UpperBody", bone, 0.0)

# Set upper body to 1 (full blend)
upper_body_bones = ["spine_01", "spine_02", "spine_03", "clavicle_l", "clavicle_r"]
for bone in upper_body_bones:
    unreal.SkeletonService.set_blend_profile_bone(skeleton_path, "UpperBody", bone, 1.0)

unreal.SkeletonService.save_asset(skeleton_path)
```

### Set Up Morph Target Curves

```python
import unreal

skeleton_path = "/Game/Characters/SK_Character"

# Add facial expression curves
facial_curves = ["BrowsUp", "BrowsDown", "EyesClosed", "Smile", "Frown"]
for curve in facial_curves:
    unreal.SkeletonService.add_curve_metadata(skeleton_path, curve)
    unreal.SkeletonService.set_curve_morph_target(skeleton_path, curve, True)

# Add material parameter curves
material_curves = ["EyeGlow", "SkinWetness", "BlushAmount"]
for curve in material_curves:
    unreal.SkeletonService.add_curve_metadata(skeleton_path, curve)
    unreal.SkeletonService.set_curve_material(skeleton_path, curve, True)

unreal.SkeletonService.save_asset(skeleton_path)
```

### Find Bones by Pattern (filter engine bone list)

There is no `find_bones` — get the full list from the engine `get_bone_names` and filter in Python:

```python
names = call_tool("editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools",
                  "get_bone_names", {"mesh": {"refPath": "/Game/Characters/SKM_Mannequin"}})
hand_bones  = [n for n in names if "hand"  in n.lower()]
twist_bones = [n for n in names if "twist" in n.lower()]
ik_bones    = [n for n in names if n.lower().startswith("ik_")]
```

### Get Full Skeleton Info

```python
import unreal

skeleton_path = "/Game/Characters/SK_Mannequin"

info = unreal.SkeletonService.get_skeleton_info(skeleton_path)
print(f"Skeleton: {info.skeleton_name}")
print(f"  Bones: {info.bone_count}")
print(f"  Compatible Skeletons: {info.compatible_skeleton_count}")
print(f"  Curve Metadata: {info.curve_meta_data_count}")
print(f"  Blend Profiles: {info.blend_profile_count}")
if info.blend_profile_names:
    print(f"    Profiles: {', '.join(info.blend_profile_names)}")
```

## Data Structures

> **Python Naming Convention**: C++ types like `FBoneNodeInfo` are exposed as `BoneNodeInfo` in Python (no `F` prefix).

### BoneNodeInfo
| Property | Type | Description |
|----------|------|-------------|
| `bone_name` | string | Name of the bone |
| `bone_index` | int | Index in hierarchy |
| `parent_bone_name` | string | Parent bone (empty = root) |
| `parent_bone_index` | int | Parent index (-1 = root) |
| `child_count` | int | Number of direct children |
| `depth` | int | Depth in hierarchy (0 = root) |
| `local_transform` | Transform | Transform relative to parent |
| `global_transform` | Transform | World-space transform |
| `retargeting_mode` | string | Translation retarget mode |
| `children` | array[string] | Names of child bones |

### SkeletonAssetInfo
| Property | Type | Description |
|----------|------|-------------|
| `skeleton_path` | string | Asset path |
| `skeleton_name` | string | Display name |
| `bone_count` | int | Number of bones |
| `compatible_skeleton_count` | int | Compatible skeletons |
| `curve_meta_data_count` | int | Curve metadata entries |
| `blend_profile_count` | int | Blend profiles |
| `blend_profile_names` | array[string] | Profile names |
| `preview_forward_axis` | string | Forward axis setting |

### SkeletalMeshData
| Property | Type | Description |
|----------|------|-------------|
| `mesh_path` | string | Asset path |
| `mesh_name` | string | Display name |
| `skeleton_path` | string | Associated skeleton |
| `bone_count` | int | Number of bones |
| `lod_count` | int | LOD levels |
| `socket_count` | int | Number of sockets |
| `morph_target_count` | int | Morph targets |
| `material_count` | int | Materials |
| `physics_asset_path` | string | Physics asset |
| `post_process_anim_bp_path` | string | Post-process AnimBP |

## Common Mistakes

### ❌ WRONG: Forgetting to commit bone changes
```python
unreal.SkeletonService.add_bone(path, "new_bone", "parent", transform)
# Bone is NOT added yet - changes lost!
```

### ✅ RIGHT: Always commit bone changes
```python
unreal.SkeletonService.add_bone(path, "new_bone", "parent", transform)
unreal.SkeletonService.commit_bone_changes(path)  # NOW it's applied
```

### ❌ WRONG: Calling `unreal.SkeletonService.add_socket(...)`
That method was cut — sockets are now engine `SkeletalMeshTools`:
```python
# WRONG: unreal.SkeletonService.add_socket(...)               # no longer exists
# RIGHT: call_tool("editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools",
#                  "add_socket", {"mesh": {"refPath": "/Game/SKM_Character"}, ...})
```

### ❌ WRONG: Not saving after VibeUE modifications
```python
unreal.SkeletonService.set_bone_retargeting_mode(path, "pelvis", "Skeleton")
# Changes in memory only - lost on editor close!
```

### ✅ RIGHT: Save after modifications
```python
unreal.SkeletonService.set_bone_retargeting_mode(path, "pelvis", "Skeleton")
unreal.SkeletonService.save_asset(path)
```

## Related

- Engine `SkeletalMeshTools` (via `call_tool`) — sockets, bone enumeration, physics asset, materials, LODs, morph targets.
- **animation-editing / animsequence** — bone-rotation editing on animations using skeleton profiles.
