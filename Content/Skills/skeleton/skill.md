---
name: skeleton
display_name: Skeleton & Skeletal Mesh Management
description: Manipulate skeletons, bones, sockets, retargeting, curve metadata, and blend profiles
vibeue_classes:
  - SkeletonService
unreal_classes:
  - Skeleton
  - SkeletalMesh
  - SkeletalMeshSocket
  - SkeletonModifier
  - BlendProfile
keywords:
  - skeleton
  - skeletal mesh
  - bone
  - socket
  - retarget
  - curve
  - blend profile
  - morph target
  - physics asset
---

# Skeleton & Skeletal Mesh Skill

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

### Socket Types: Mesh vs Skeleton

- **Mesh Socket** (`bAddToSkeleton=False`): Specific to one skeletal mesh
- **Skeleton Socket** (`bAddToSkeleton=True`): Shared across ALL meshes using that skeleton

```python
# Mesh-specific socket (only on this mesh)
unreal.SkeletonService.add_socket(
    "/Game/SKM_Mannequin",
    "Helmet_Attach",
    "head",
    unreal.Vector(0, 0, 20),
    unreal.Rotator(0, 0, 0),
    unreal.Vector(1, 1, 1),
    False  # Mesh-only
)

# Skeleton-wide socket (shared with all meshes)
unreal.SkeletonService.add_socket(
    "/Game/SKM_Mannequin",
    "Weapon_R",
    "hand_r",
    unreal.Vector(10, 0, 0),
    unreal.Rotator(0, 0, 90),
    unreal.Vector(1, 1, 1),
    True  # Add to skeleton
)
```

### Skeleton vs SkeletalMesh Paths

- **Skeleton** operations (retargeting, curves, blend profiles) use `SK_` assets
- **SkeletalMesh** operations (sockets, bone modification) use `SKM_` assets

```python
# Skeleton operations - use SK_ path
unreal.SkeletonService.get_skeleton_info("/Game/Characters/SK_Mannequin")
unreal.SkeletonService.set_bone_retargeting_mode("/Game/Characters/SK_Mannequin", "pelvis", "Skeleton")

# SkeletalMesh operations - use SKM_ path
unreal.SkeletonService.list_sockets("/Game/Characters/SKM_Mannequin")
unreal.SkeletonService.add_socket("/Game/Characters/SKM_Mannequin", ...)
```

### Retargeting Mode Reference

| Mode | Description | Use For |
|------|-------------|---------|
| `Animation` | Use animation data directly | Most bones (default) |
| `Skeleton` | Use skeleton's reference pose | Root, pelvis, IK targets |
| `AnimationScaled` | Scale animation by skeleton size | Height-scaled characters |
| `OrientAndScale` | Match orientation and scale | Helper/utility bones |

## Workflows

### List All Bones with Hierarchy

```python
import unreal

bones = unreal.SkeletonService.list_bones("/Game/Characters/SKM_Mannequin")
for bone in bones:
    indent = "  " * bone.depth
    children = f" [{bone.child_count} children]" if bone.child_count > 0 else ""
    print(f"{indent}{bone.bone_name}{children}")
```

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

### Set Up Weapon Sockets

```python
import unreal

mesh_path = "/Game/Characters/SKM_Character"

# Right hand weapon socket
unreal.SkeletonService.add_socket(
    mesh_path,
    "Weapon_R",
    "hand_r",
    unreal.Vector(10.0, 0.0, 0.0),
    unreal.Rotator(0.0, 0.0, 90.0),
    unreal.Vector(1.0, 1.0, 1.0),
    True  # Add to skeleton for sharing
)

# Left hand weapon socket
unreal.SkeletonService.add_socket(
    mesh_path,
    "Weapon_L",
    "hand_l",
    unreal.Vector(10.0, 0.0, 0.0),
    unreal.Rotator(0.0, 0.0, -90.0),
    unreal.Vector(1.0, 1.0, 1.0),
    True
)

# Back holster
unreal.SkeletonService.add_socket(
    mesh_path,
    "Holster_Back",
    "spine_03",
    unreal.Vector(0.0, -20.0, 0.0),
    unreal.Rotator(0.0, 90.0, 0.0),
    unreal.Vector(1.0, 1.0, 1.0),
    True
)

unreal.SkeletonService.save_asset(mesh_path)
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

### Find Bones by Pattern

```python
import unreal

mesh_path = "/Game/Characters/SKM_Mannequin"

# Find all hand-related bones
hand_bones = unreal.SkeletonService.find_bones(mesh_path, "hand")
print(f"Hand bones: {hand_bones}")

# Find all twist bones
twist_bones = unreal.SkeletonService.find_bones(mesh_path, "twist")
print(f"Twist bones: {twist_bones}")

# Find all IK bones
ik_bones = unreal.SkeletonService.find_bones(mesh_path, "ik_")
print(f"IK bones: {ik_bones}")
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

### MeshSocketInfo
| Property | Type | Description |
|----------|------|-------------|
| `socket_name` | string | Socket identifier |
| `bone_name` | string | Attached bone |
| `relative_location` | Vector | Offset from bone |
| `relative_rotation` | Rotator | Rotation from bone |
| `relative_scale` | Vector | Scale factor |
| `force_always_animated` | bool | Force bone LOD |

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

### ❌ WRONG: Using Skeleton path for socket operations
```python
unreal.SkeletonService.add_socket("/Game/SK_Character", ...)  # FAILS!
```

### ✅ RIGHT: Use SkeletalMesh path for sockets
```python
unreal.SkeletonService.add_socket("/Game/SKM_Character", ...)  # Correct
```

### ❌ WRONG: Not saving after modifications
```python
unreal.SkeletonService.add_socket(path, "Socket", "bone", ...)
# Changes in memory only - lost on editor close!
```

### ✅ RIGHT: Save after modifications
```python
unreal.SkeletonService.add_socket(path, "Socket", "bone", ...)
unreal.SkeletonService.save_asset(path)
```
