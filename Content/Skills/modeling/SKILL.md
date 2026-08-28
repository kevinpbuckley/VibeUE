---
name: modeling
display_name: Mesh Modeling
description: Create and edit static/skeletal mesh geometry programmatically (ModelingService) — primitives, booleans, extrude/inset/bevel on selections, remesh/simplify/subdivide, deform, voxel ops, auto-UV, normals, repair, collision, LODs, bake, and saving to assets. Use when the user asks to build, kitbash, block out, fix, clean up, or generate 3D meshes in the editor without Blender.
vibeue_classes:
  - ModelingService
unreal_classes:
  - DynamicMesh
  - StaticMesh
  - GeometryScript_MeshPrimitives
  - GeometryScript_MeshBooleans
  - GeometryScript_MeshUVs
  - StaticMeshEditorSubsystem
keywords:
  - modeling
  - mesh
  - geometry
  - geometry script
  - primitive
  - boolean
  - extrude
  - inset
  - bevel
  - remesh
  - simplify
  - subdivide
  - voxel
  - collision
  - lod
  - bake normal map
  - kitbash
  - blockout
  - procedural mesh
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "meshes-static-and-skeletal"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Mesh Modeling Skill

`ModelingService` (`unreal.ModelingService`) is the programmatic half of Unreal's **Modeling Mode**:
the same operators the 97 interactive tools are built on, exposed as functions on an in-memory
`DynamicMesh` that you build up and then save as a `StaticMesh` / `SkeletalMesh` asset. Nothing
here needs a viewport, a gizmo, or Blender.

**Mental model — a session of mesh handles:**

1. `create_mesh()` / `load_mesh_from_static_mesh(path)` / `load_mesh_from_actor(label)` → an integer **handle**.
2. Every op mutates the handle's mesh in place and returns an `FModelingResult` (`success`, `message`, `triangle_count`, `vertex_count`).
3. `save_mesh_to_static_mesh(handle, "/Game/Props/SM_Crate")` writes the asset (creates or replaces); `spawn_static_mesh_actor(...)` places it.
4. `release_mesh(handle)` when done (or `release_all_meshes()`); handles are session-scoped and not saved.

Operations that take a **selection** use a named selection created on the same handle
(`select_all`, `select_by_normal_angle`, `select_in_box`, `select_in_sphere`, `select_by_material_id`,
`expand_contract_selection`, `invert_selection`). Pass `""` as the selection name to mean "the whole mesh".

## Quick start — a crate with a lid seam, UVs, collision, LODs

```python
import unreal
svc = unreal.ModelingService
h = svc.create_mesh()
svc.append_box(h, unreal.Transform(), 100, 100, 100)                       # body
lid = svc.create_mesh()
svc.append_box(lid, unreal.Transform(unreal.Vector(0, 0, 90)), 104, 104, 4) # lid slab
svc.boolean(h, lid, "Union")
svc.select_by_normal_angle(h, "top", unreal.Vector(0, 0, 1), 5.0)
svc.inset_faces(h, "top", 6.0)                                              # panel line on the lid
svc.extrude_faces(h, "top", -1.5)                                           # recess it
svc.bevel_polygroups(h, 0.8)
svc.recompute_normals(h, 30.0)
svc.auto_uv(h, "PatchBuilder", 0)
r = svc.save_mesh_to_static_mesh(h, "/Game/Props/SM_Crate", True, True)     # nanite off, collision on
print("CREATED:", r.asset_path, r.triangle_count, "tris")
svc.generate_collision("/Game/Props/SM_Crate", "ConvexHulls", 4)
svc.set_lods("/Game/Props/SM_Crate", [1.0, 0.5, 0.25])
svc.spawn_static_mesh_actor("/Game/Props/SM_Crate", unreal.Transform(unreal.Vector(0, 0, 0)), "Crate_01")
svc.release_all_meshes()
```

Then **look at it**: `call_tool(tool_name="CaptureViewport", toolset_name="EditorToolset.EditorAppToolset")`
framed on the actor. A successful call is not proof the shape is right.

## Function map (Python names)

| Area | Functions |
|---|---|
| Session | `create_mesh()`, `load_mesh_from_static_mesh(path, lod=0)`, `load_mesh_from_skeletal_mesh(path, lod=0)`, `load_mesh_from_actor(label)`, `copy_mesh(handle)`, `release_mesh(handle)`, `release_all_meshes()`, `list_meshes()`, `get_mesh_info(handle)` |
| Primitives (append into a handle) | `append_box`, `append_sphere`, `append_cylinder`, `append_cone`, `append_capsule`, `append_torus`, `append_rectangle`, `append_disc`, `append_stairs`, `append_extrude_polygon(handle, points2d, height)`, `append_revolve_polygon(handle, points2d, radius, steps, degrees)`, `append_mesh(handle, other_handle, transform)` |
| Booleans | `boolean(target, tool, "Union|Subtract|Intersection", tool_transform)`, `self_union`, `plane_cut(handle, plane_transform, fill_holes)`, `mirror(handle, plane_transform, weld)` |
| Selections | `select_all(handle, name)`, `select_by_normal_angle(handle, name, normal, max_angle_deg)`, `select_in_box(handle, name, box_min, box_max)`, `select_in_sphere(handle, name, center, radius)`, `select_by_material_id(handle, name, material_id)`, `expand_contract_selection(handle, name, iterations, contract)`, `invert_selection(handle, name)`, `selection_count(handle, name)` |
| Poly edit on selections | `extrude_faces(handle, sel, distance, direction)`, `offset_faces(handle, sel, distance)`, `inset_faces(handle, sel, distance)`, `outset_faces(handle, sel, distance)`, `delete_faces(handle, sel)`, `translate_selection(handle, sel, delta)`, `bevel_polygroups(handle, distance, subdivisions)`, `offset_mesh(handle, distance)`, `shell_mesh(handle, thickness)` |
| Mesh | `remesh(handle, target_triangles, edge_length=0)`, `simplify_to_triangle_count(handle, n)`, `simplify_to_tolerance(handle, tolerance)`, `subdivide(handle, level, "PN|Uniform|CatmullClark|Loop")`, `smooth(handle, sel, iterations, alpha)`, `fill_holes(handle)`, `weld_edges(handle, tolerance)`, `repair(handle)` (degenerates + small components), `remove_hidden_triangles(handle)`, `split_by_components(handle)` → new handles |
| Deform | `bend(handle, orientation, angle, extent)`, `twist(...)`, `flare(handle, orientation, percent_x, percent_y, extent)`, `noise(handle, sel, magnitude, frequency, seed)`, `displace_from_texture(handle, sel, texture_path, magnitude, uv_layer)` |
| Voxel | `voxel_solidify(handle, grid_resolution)`, `voxel_morphology(handle, "Dilate|Contract|Open|Close", distance, grid_resolution)` |
| UVs / normals / groups / materials / colors | `auto_uv(handle, "XAtlas|PatchBuilder", uv_layer)`, `project_uv(handle, "Planar|Box|Cylinder", transform, uv_layer, sel)`, `repack_uv(handle, uv_layer, resolution)`, `set_num_uv_layers`, `recompute_normals(handle, hard_angle_deg)`, `flip_normals`, `compute_polygroups(handle, crease_angle)`, `set_material_id(handle, sel, id)`, `set_vertex_color(handle, sel, color)` |
| Transform | `transform_mesh(handle, transform)`, `translate_mesh`, `rotate_mesh`, `scale_mesh`, `recenter_mesh(handle, "Bounds|Base")` |
| Assets | `save_mesh_to_static_mesh(handle, path, replace_existing, enable_collision, enable_nanite)`, `save_mesh_to_skeletal_mesh(handle, path, skeleton_path)`, `transfer_bone_weights(handle, source_skeletal_mesh_path)`, `generate_collision(asset_path, "MinVolumeShapes|ConvexHulls|AlignedBoxes|OrientedBoxes|MinimalSpheres|Capsules|SweptHulls", max_hulls)`, `set_lods(asset_path, [percent_triangles...])`, `bake_textures(target_handle, source_handle, "TangentNormal,AmbientOcclusion,Curvature,ObjectNormal,Position", resolution, out_folder, base_name)`, `spawn_static_mesh_actor(asset_path, transform, label)` |

Exact signatures: `discover_python_class('unreal.ModelingService')`. The long tail — anything not
here — is reachable directly: the mesh behind a handle is `svc.get_dynamic_mesh(handle)`, which you
can pass to any `unreal.GeometryScript_*` library function in the same script.

## Critical rules

- **Units are centimeters, +Z up, origin at the base by default** for `append_box`/`append_cylinder`
  (`origin="Center"` to change). A 100 cm crate at `Transform()` sits on the floor.
- **Booleans want closed meshes.** Run `get_mesh_info` — `is_closed` false or `open_border_edges > 0`
  means fill holes / weld first, or the boolean produces flaps. `self_union` cleans up kitbash overlaps.
- **Selections are recomputed, not tracked.** After a topology-changing op (extrude, boolean, remesh)
  re-run the `select_*` call; old selections may point at triangles that no longer exist.
- **Order for a clean asset:** model → `repair` → `recompute_normals` → `auto_uv` → `save` →
  `generate_collision` → `set_lods`. UVs before save, collision and LODs after (they act on the asset).
- **`save_mesh_to_static_mesh` replaces LOD0 of an existing asset** when `replace_existing` is true;
  its materials are kept. Existing collision/LODs on that asset are not touched — regenerate them if
  the silhouette changed.
- **Log for rollback:** `print("CREATED:", path)` after each save; there is no undo for handle ops
  (they are in memory), but assets go through the editor and `TransactionService` checkpoints apply.
- **Verify with evidence.** `get_mesh_info` after each stage (triangle/vertex counts, bounds, closed),
  and a viewport capture once it is in the level.
- **Budget triangles.** Voxel ops and PN tessellation explode counts; follow them with
  `simplify_to_triangle_count`. Keep props under ~20k tris unless Nanite is on.

## Common mistakes

- Calling `extrude_faces` with `""` (whole mesh) — that shells the whole object outward. Select faces first.
- Forgetting `auto_uv` before `bake_textures` or before saving a mesh that will take a textured material.
- `set_lods` with `[1.0]` only — that removes extra LODs. Give one entry per LOD, LOD0 first.
- Loading a mesh from an actor and saving back to a *new* path, then wondering why the level didn't change — spawn or reassign the actor's mesh.

## Return types

- `FModelingResult`: `success`, `message`, `handle`, `triangle_count`, `vertex_count`, `asset_path`.
- `FModelingMeshInfo` (`get_mesh_info`): `handle`, `triangle_count`, `vertex_count`, `is_closed`, `open_border_edges`, `connected_components`, `bounds_min`, `bounds_max`, `surface_area`, `volume`, `num_uv_layers`, `has_vertex_colors`, `material_ids`.
- `FModelingSplitResult` (`split_by_components`): `success`, `handles`.
