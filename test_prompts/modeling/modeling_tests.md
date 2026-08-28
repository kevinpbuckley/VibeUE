# Mesh Modeling Test Prompts

Sequential prompts that exercise every `ModelingService` function through an agent. Each section
names the functions it covers; the agent should call them via `execute_python_code`
(`unreal.ModelingService`) after loading the `modeling` skill. Assets are created under
`/Game/Developers/VibeUEModelingTests` and removed in the last section.

For a no-agent check of the same surface, run `Content/Skills/modeling/scripts/exercise_all.txt`
(calls every function and prints `RESULT: PASS`), or the automation tests `VibeUE.Modeling.*`
(console: `Automation RunTests VibeUE.Modeling`).

## Prerequisites

- UE 5.8 editor with VibeUE enabled and the MCP server running.
- No assets required; the Skeletal section needs any SkeletalMesh in the project (skip otherwise).

## Test Order

1. Session basics
2. Primitives
3. Booleans
4. Selections and poly-edit
5. Mesh processing
6. Deformation and voxels
7. UVs, normals, groups, materials, colors
8. Transforms
9. Queries, sampling, hulls, comparison
10. Assets, collision, LODs, baking, placement
11. Skeletal
12. Error handling
13. Cleanup

---

# 1. Session basics

## Goal
Handles are created, inspected, copied, listed, and released. Covers `create_mesh`, `copy_mesh`,
`list_meshes`, `get_mesh_info`, `get_dynamic_mesh`, `release_mesh`, `release_all_meshes`.

## Prompt 1.1
Create an empty modeling mesh, append a 100 cm box to it, and tell me its handle, triangle count, vertex count, whether it is closed, and its bounds.

## Prompt 1.2
Copy that mesh to a second handle, list all live handles, then release the copy and list again. Confirm the original still reports 12 triangles.

## Prompt 1.3
Get the raw DynamicMesh for the handle and use a GeometryScript function directly on it (e.g. `unreal.GeometryScript_MeshQueries` for the bounding box) to show the long-tail path works.

---

# 2. Primitives

## Goal
Every primitive appends geometry. Covers `append_box`, `append_sphere`, `append_sphere_box`,
`append_cylinder`, `append_cone`, `append_capsule`, `append_torus`, `append_rectangle`,
`append_disc`, `append_stairs`, `append_curved_stairs`, `append_extrude_polygon`,
`append_revolve_polygon`, `append_sweep_polyline`, `append_mesh`.

## Prompt 2.1
Into one handle, append a box, a lat-long sphere, a sphere-box, a cylinder, a cone, a capsule, and a torus, each 120 cm apart along X. Report the triangle count after each append.

## Prompt 2.2
Into a new handle append a rectangle, a disc with a 10 cm hole and a 270° arc, an 8-step straight staircase, and a 6-step curved staircase with 80 cm inner radius.

## Prompt 2.3
Extrude a 2D L-shaped polygon 15 cm along Z, and revolve a vase profile (points in X = radius, Y = height) around Z with 24 steps.

## Prompt 2.4
Sweep a small V-shaped profile along a 3-frame path that rises and turns, then append that whole handle into the handle from 2.1 offset 200 cm up.

---

# 3. Booleans

## Goal
Covers `boolean` (Union / Subtract / Intersection), `self_union`, `plane_cut`, `mirror`.

## Prompt 3.1
Make a 100 cm cube and a 40 cm radius sphere at one corner. Subtract the sphere from the cube, then check `get_mesh_info` still reports a closed mesh.

## Prompt 3.2
Union two overlapping boxes, run self-union to clean the overlap, then plane-cut at Z = 80 with hole filling and report open border edges (should be 0).

## Prompt 3.3
Mirror a half-model across the YZ plane with plane-cut and weld enabled, and confirm the bounds became symmetric.

---

# 4. Selections and poly-edit

## Goal
Covers `select_all`, `select_by_normal_angle`, `select_in_box`, `select_in_sphere`,
`select_by_material_id`, `select_by_polygroup`, `expand_contract_selection`, `invert_selection`,
`selection_count`, `selection_bounds`, `clear_selections`, `extrude_faces`, `offset_faces`,
`inset_faces`, `outset_faces`, `delete_faces`, `translate_selection`, `bevel_polygroups`,
`offset_mesh`, `shell_mesh`.

## Prompt 4.1
On a 100 cm cube select the top faces by normal (within 5°), report the count (2 triangles) and the selection bounds, inset them 6 cm, reselect the top, and extrude it -1.5 cm to make a recessed lid panel.

## Prompt 4.2
Select the +X side, offset those faces 2 cm, translate the selection 1 cm along X, set material ID 3 on it, then select by material ID 3 and confirm the count matches.

## Prompt 4.3
Compute polygroups by angle, select polygroup 1, expand the selection by one ring, invert it, and report the three counts. Then bevel the polygroup edges by 0.5 cm and recompute normals with a 30° hard angle.

## Prompt 4.4
Select the bottom faces, delete them, fill the hole, and confirm the mesh is closed again. Then clear all selections.

## Prompt 4.5
Make a flat 50 cm rectangle, shell it to 2 cm thickness, then offset the whole surface outward 1 cm.

---

# 5. Mesh processing

## Goal
Covers `remesh`, `simplify_to_triangle_count`, `simplify_to_vertex_count`, `simplify_to_tolerance`,
`simplify_planar`, `subdivide` (PN / Uniform / CatmullClark / Loop), `smooth`, `fill_holes`,
`weld_edges`, `repair`, `remove_hidden_triangles`, `split_by_components`.

## Prompt 5.1
Take a 40 cm sphere, remesh to 1500 triangles, simplify to 600 triangles, then to 250 vertices, then to a 0.5 cm tolerance; report counts after each step.

## Prompt 5.2
Subdivide a box once with each method: PN, Uniform, Loop, and — after computing polygroups with the Polygons method — CatmullClark. Then run planar simplify on the CatmullClark result.

## Prompt 5.3
Append two disjoint boxes into one handle, run repair and remove-hidden-triangles, then split by components into separate handles and report their triangle counts.

---

# 6. Deformation and voxels

## Goal
Covers `bend`, `twist`, `flare`, `noise`, `displace_from_texture`, `voxel_solidify`,
`voxel_morphology`.

## Prompt 6.1
Make a 100 cm tall cylinder with 8 height steps; bend it 30°, twist it 30°, and flare it 20% about its midpoint, reporting bounds after each.

## Prompt 6.2
Apply Perlin noise (magnitude 2, frequency 0.1) to a sphere, voxel-solidify it at grid 32, then run a voxel Close of 2 cm, and confirm the result is closed.

## Prompt 6.3
Find any Texture2D in the project and displace a subdivided plane along its normals by 10 cm using that texture in UV layer 0.

---

# 7. UVs, normals, groups, materials, colors

## Goal
Covers `auto_uv` (XAtlas / PatchBuilder), `project_uv` (Planar / Box / Cylinder), `repack_uv`,
`set_num_uv_layers`, `recompute_normals`, `flip_normals`, `compute_polygroups` (Angle / UVIslands /
Components / Polygons), `set_material_id`, `remap_material_id`, `set_vertex_color`.

## Prompt 7.1
Give a box two UV layers: XAtlas-unwrap layer 0 and PatchBuilder-unwrap layer 1; then box-project layer 0 and repack it at 512.

## Prompt 7.2
Recompute normals fully smooth, then with a 30° hard angle, flip them and flip them back.

## Prompt 7.3
Compute polygroups from UV islands and from components, set material ID 2 on the whole mesh, remap 2 → 1, paint the whole mesh white and the top faces green.

---

# 8. Transforms

## Goal
Covers `transform_mesh`, `translate_mesh`, `rotate_mesh`, `scale_mesh`, `recenter_mesh`.

## Prompt 8.1
Translate a mesh 10 cm on X and back, rotate it 45° yaw about the origin, scale it 2× on Z, then recenter to Bounds and finally to Base; confirm the bottom sits at Z = 0.

---

# 9. Queries, sampling, hulls, comparison

## Goal
Covers `ray_cast`, `nearest_point`, `is_point_inside`, `sample_surface_points`, `convex_hull`,
`convex_decomposition`, `swept_hull`, `measure_distance`.

## Prompt 9.1
On a 50 cm sphere, cast a ray from (-200,0,0) along +X and report the hit position and triangle; find the nearest surface point to (0,0,120); test whether the origin and (0,0,500) are inside.

## Prompt 9.2
Scatter Poisson-disc samples with a 15 cm radius on the sphere and report how many you got and the first three transforms.

## Prompt 9.3
Build the convex hull, a 2-hull convex decomposition, and a swept hull along Z of the boolean result from 3.1, each as new handles, and measure the distance between the sphere and its convex hull (max should be small).

---

# 10. Assets, collision, LODs, baking, placement

## Goal
Covers `save_mesh_to_static_mesh` (create and replace), `load_mesh_from_static_mesh`,
`generate_collision`, `set_lods`, `spawn_static_mesh_actor`, `load_mesh_from_actor`,
`bake_textures`.

## Prompt 10.1
Save the crate from section 4 as `/Game/Developers/VibeUEModelingTests/SM_Crate` with collision enabled and Nanite off; then save again to the same path to prove replace works, and load it back into a new handle.

## Prompt 10.2
Generate ConvexHulls collision (max 4 hulls) on that asset and set a 3-entry LOD chain (100 %, 50 %, 25 %). Report the LOD count.

## Prompt 10.3
Spawn the asset as an actor labeled `ModelingTestCrate`, take a viewport capture framed on it, then load the mesh back from the actor in world space.

## Prompt 10.4
Bake a 64 px tangent-space normal map and ambient occlusion from the noisy sphere onto the UV'd box into `/Game/Developers/VibeUEModelingTests`, and report the texture asset paths.

---

# 11. Skeletal (needs any SkeletalMesh in the project)

## Goal
Covers `load_mesh_from_skeletal_mesh`, `transfer_bone_weights`, `smooth_bone_weights`,
`prune_bone_weights`, `save_mesh_to_skeletal_mesh`.

## Prompt 11.1
Find a SkeletalMesh in the project, load its LOD 0 into a handle, remesh it to 5000 triangles, transfer bone weights back from the source asset, smooth them against its skeleton, and save as `/Game/Developers/VibeUEModelingTests/SK_Remeshed` bound to the same skeleton.

## Prompt 11.2
Prune the `root` bone from the remeshed mesh's skin weights and report the result message.

---

# 12. Error handling

## Prompt 12.1
Call a boolean with the operation `Frobnicate` and an append on handle 999, and show me the exact error messages (they should list the valid operations and tell you how to create a mesh).

## Prompt 12.2
Try `inset_faces` with a selection name that does not exist and confirm the error names the missing selection.

---

# 13. Cleanup

## Prompt 13.1
Delete the `ModelingTestCrate` actor, delete everything under `/Game/Developers/VibeUEModelingTests`, and release all modeling handles. Confirm `list_meshes` is empty.

## Prompt 14.1 — lofted wing

**Prompt:** "Loft a tapered wing section: a symmetric airfoil profile, root chord 300 at the origin, tip chord 120 four metres out along +Y and swept back 80 cm. Prove it is closed and faces outward, then save it as `/Game/Developers/Test/SM_LoftWing`."

**Expected:** `append_loft` with two frames (yaw 90, scale = chord); `get_mesh_info` shows `is_closed` and a positive volume; a saved static mesh. No manual `fill_holes` / `flip_normals` needed.

## Prompt 14.2 — rigged rudder

**Prompt:** "Build a simple tail fin with a rudder: a fin slab, a through-slot cut along the hinge line so the rear third is a separate piece. Create a `root` bone and a `rudder` bone whose X axis runs along the hinge, bind the rudder piece to it, confirm with `list_bones` that the rudder bone owns exactly the rudder's vertices, and save it as `/Game/Developers/Test/SK_Fin` with a new skeleton."

**Expected:** `boolean` Subtract for the slot → `select_connected` on a point in the rudder → `create_bones` (root, rudder) → `bind_selection_to_bone` → `list_bones` showing two bones with non-zero counts → `save_mesh_to_skeletal_mesh(..., "", ...)` producing `SK_Fin` and `SK_Fin_Skeleton`.
