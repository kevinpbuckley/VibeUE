---
name: uv-mapping
display_name: UV Mapping
description: Inspect, generate, transform, and visualize UV channels on StaticMesh assets via UVMappingService
vibeue_classes:
  - UVMappingService
unreal_classes:
  - StaticMesh
  - MeshDescription
  - StaticMeshAttributes
  - StaticMeshOperations
keywords:
  - uv
  - uvs
  - uv mapping
  - uv channel
  - lightmap
  - lightmap uvs
  - unwrap
  - auto unwrap
  - texel density
  - uv overlap
  - uv layout
  - retopo uvs
  - texture coordinates
  - uv pack
---

# UV Mapping Skill

`UVMappingService` provides automation-grade UV channel manipulation for StaticMesh assets in UE 5.7+. The service is built around mesh-description editing and post-build commits — every mutation marks the package dirty and rebuilds render data, but **does not save**. Follow with `manage_asset(action="save", ...)` once you finish a batch.

## When NOT to Use This Service

UV transforms are the **wrong tool** for fixing a tiling material that looks bad on a basic shape (cube, sphere, cylinder, cone). Symptoms that say "this is not a UV problem":

- "The bricks face the wrong way on this face."
- "The bricks tile fine on the side but look stretched on the cap."
- "It tiles correctly on one shape but not another with the same material."

These are **material-orientation problems**, not UV-layout problems. A single 2D UV transform can't satisfy multiple face orientations on a 3D shape — whichever projection you pick, at least one face will be wrong. **Reach for one of these instead:**

1. **Per-actor child Material Instance with `Tiling` overridden.** If the parent material exposes a `Tiling` scalar (Megascans `M_MS_Srf` does), create a child MI with the right multiplier and assign it to the specific actor. The mesh keeps its stock UVs.
2. **World-aligned / triplanar material.** A material that samples textures projected from world axes and blends by world normal. Every face on every shape shows correctly oriented bricks at the right size, regardless of mesh UVs. UE provides `/Engine/Functions/Engine_MaterialFunctions02/Texturing/WorldAlignedTexture` and `WorldAlignedNormal` for this.

`UVMappingService` is the right tool for:

- Generating lightmap UVs / fixing lightmap overlaps
- Repairing imported broken UVs (no UVs at all, gigantic UVs, scrambled layouts)
- Adding a detail-texture UV channel that needs different tiling than channel 0
- Texel-density auditing
- UV layout export for documentation
- Channel lifecycle (add / remove / copy / set count)

If the user asks to "fix UVs" on a basic shape with a tiling material, ask whether they want material-side or mesh-side first — defaulting to mesh-side will burn iterations.

## Critical Rules

### StaticMesh Only (Mostly)

Detailed inspection and all mutating operations target `UStaticMesh`. SkeletalMesh meshes are accepted by `list_uv_channels` and `mesh_has_uv_channel` for read-only channel counts but **all other methods will fail** on skeletal meshes — the editing path goes through `FSkeletalMeshLODModel` and is not yet implemented. If you're prompted to UV-map a skeletal mesh, stop and tell the user.

### Save After Edits

`UVMappingService` calls `MarkPackageDirty()` and `PostEditChange()` on every successful mutation but does not save. After a batch of edits:

```python
unreal.UVMappingService.set_lightmap_settings("/Game/Meshes/SM_Wall", 1, 0, 128, True)
unreal.UVMappingService.generate_lightmap_uvs("/Game/Meshes/SM_Wall", 0, 1, 1.0)
# Save once at the end:
manage_asset(action="save", asset_path="/Game/Meshes/SM_Wall")
```

### Health Check Before Editing

Always run `get_uv_health` first. It is a single call that tells you channel count, lightmap config, overlap status, and texel density per channel. This prevents wasted work like generating lightmap UVs into an index that already has good UVs.

### Lightmap Channel Convention

By UE convention:
- **Channel 0** holds the texture/material UVs.
- **Channel 1** is the lightmap UV channel.
- `LightMapCoordinateIndex` on the static mesh **must** point at the lightmap channel.

`generate_lightmap_uvs` automatically sets `LightMapCoordinateIndex = DestUVIndex` for you. `remove_uv_channel` adjusts it if you remove a channel above or at the lightmap.

### Channel 0 Cannot Be Removed

`remove_uv_channel` rejects channel 0 — every static mesh must have at least one UV channel. To replace channel 0's contents, use `auto_unwrap_uvs` or `copy_uv_channel`.

### MAX_MESH_TEXTURE_COORDS_MD = 8

Static meshes support at most 8 UV channels. `add_uv_channel` returns failure beyond this limit; `set_uv_channel_count` rejects counts > 8.

### Overlap Detection Is Heuristic

`OverlapPercent` uses an O(N²) AABB-overlap heuristic on triangle UV bounding boxes, capped at 5,000 triangles. Above the cap, the field returns `-1.0` to signal "skipped." It produces conservative false positives (touching but non-overlapping AABBs) — use it as a smoke test, not a precise measurement.

### Pack Is Fit-To-Square, Not True Island Packing

`pack_uvs` rescales the entire channel's UV bounds into `[padding, 1-padding]^2`. It does **not** detect connected islands and shelf-pack them. For real island packing, run `auto_unwrap_uvs` with `Box` projection first — that produces island-shaped charts the engine packs internally.

---

## Per-Region UV Edits (Artist-Grade)

For meshes where one face/region needs a different UV transform than the rest (e.g. a thin disc whose side strip distorts a wall texture while the cap looks fine), use the per-region transforms. They apply an affine transform to a *subset* of vertex instances, leaving everything else untouched.

### Filter By Vertex Normal Direction

`transform_u_vs_by_normal(mesh, lod, channel, axis_x, axis_y, axis_z, min_dot, max_dot, scale_u, scale_v, rotation_degrees, offset_u, offset_v)`

Applies the affine transform only to vertex instances whose normal, dotted with the given axis, falls in `[min_dot, max_dot]`. Common windows for axis = world +Z `(0,0,1)`:

| Window | Selects |
|---|---|
| `min_dot=0.5, max_dot=1.0` | Top (upward-facing) |
| `min_dot=-1.0, max_dot=-0.5` | Bottom (downward-facing) |
| `min_dot=-0.5, max_dot=0.5` | Sides (horizontal-pointing) |
| `min_dot=0.7, max_dot=1.0` | Strict "up" only |

`count_vertices_by_normal(mesh, lod, axis_x, axis_y, axis_z, min_dot, max_dot)` previews the selection without modifying anything — call it first to verify your filter is selecting the right region.

**Worked example — fix a thin disc's side strip aspect ratio while leaving the cap correct:**

```python
# Disc: scale (2.5, 2.5, 0.4), so world dimensions 250cm wide × 40cm tall.
# Side strip is 785cm (circumference) × 40cm (height) — 20:1 aspect.
# Cap is 250×250cm — 1:1 aspect.
# With a uniform brick material at Tiling=4, the cap renders correct bricks
# but the side renders as horizontal pinstripes.

# Step 1: confirm the filter selects only the side
side = unreal.UVMappingService.count_vertices_by_normal(
    "/Game/Meshes/SM_Disc", 0,  0,0,1,  -0.5, 0.5)
print(f"side vertex instances: {side}")  # ~960 for engine cylinder

# Step 2: scale U on the side only by 20x to bring tile aspect back to ~1:1
unreal.UVMappingService.transform_u_vs_by_normal(
    "/Game/Meshes/SM_Disc", 0, 0,
    0,0,1,        # axis = world Z
    -0.5, 0.5,    # side selector
    scale_u=20.0, scale_v=1.0,  # only U
)
manage_asset(action="save", asset_path="/Game/Meshes/SM_Disc")
```

> 💡 **Which axis to scale**: figure out the world-space aspect of the misbehaving region, then scale UVs to compensate. If a region is 20× wider than tall in world, scale U by 20× to make the brick texture tile 20× more in U than V — bricks now appear correctly proportioned.

### UV Islands (Most Robust Per-Region Filter)

Use `identify_uv_islands` + `transform_uv_island` when normal-direction or polygon-group filters can't cleanly separate the regions you want to operate on (e.g. an irregular mesh, or a UV layout where similar-normal faces are still distinct islands).

`identify_uv_islands(mesh, lod, channel)` returns an array of `FUVIslandInfo`, one per connected UV island. Two vertex instances are in the same island iff they're reachable through triangles with no UV seam between them.

```python
islands = unreal.UVMappingService.identify_uv_islands("/Game/Meshes/SM_Disc", 0, 0)
for i in islands:
    n, c = i.average_normal, i.world_center
    print(f"Island {i.island_id}: tris={i.triangle_count} VIs={i.vertex_instance_count} "
          f"bounds=({i.min_u:.2f},{i.min_v:.2f})..({i.max_u:.2f},{i.max_v:.2f}) "
          f"normal=({n.x:.2f},{n.y:.2f},{n.z:.2f}) center=({c.x:.0f},{c.y:.0f},{c.z:.0f})")
```

Each island carries its own bounds, area, average world-space center and average normal — use those fields to identify which island is which. For a simple cylinder you'll get 3 islands: top cap (normal ≈ +Z), bottom cap (normal ≈ -Z), and the side strip (largest triangle count).

Once you've identified the island, transform just it:

```python
# Example: scale only the side strip's UV U by 4x — caps and other islands untouched
side = max(islands, key=lambda i: i.triangle_count)
unreal.UVMappingService.transform_uv_island(
    "/Game/Meshes/SM_Disc", 0, 0, side.island_id,
    scale_u=4.0, scale_v=1.0)
```

Island ids are stable as long as the mesh's UV topology hasn't changed since `identify_uv_islands` was called. If you re-run an `auto_unwrap` / `pack` / structural change, call `identify_uv_islands` again for fresh ids before transforming.

### Filter By Polygon Group (Material Slot)

`transform_u_vs_by_polygon_group(mesh, lod, channel, polygon_group_name, scale_u, scale_v, rotation_degrees, offset_u, offset_v)`

Applies the transform only to triangles in the named polygon group (typically a material slot). Use when the mesh has clean per-section slots — e.g. a wall with separate `body` and `trim` material slots, where you want different tiling per slot but the same base material.

`list_polygon_groups(mesh, lod)` returns the slot names available on a LOD. The engine basic shapes have just one slot (`DefaultMaterial`), so this filter is most useful on imported meshes with authored slots.

```python
groups = unreal.UVMappingService.list_polygon_groups("/Game/Meshes/SM_Wall", 0)
# ['Body', 'Trim']
unreal.UVMappingService.transform_u_vs_by_polygon_group(
    "/Game/Meshes/SM_Wall", 0, 0, "Trim",
    scale_u=2.0, scale_v=2.0,  # 2x denser bricks on trim only
)
```

---

## Common Mistakes to Avoid

| WRONG | CORRECT |
|-------|---------|
| `result.success` returning False but no error string | Check `result.message` — every failure includes a reason |
| Generating lightmap UVs without setting `MinChartSpacingPercent` | Pass at least `1.0` for typical meshes; smaller values pack tighter but risk bleeding |
| Calling `transform_uvs` on the lightmap channel | Avoid — transforms break the channel's `[0,1]` packing. Use it on material UV channels only |
| Forgetting to save after edits | Add `manage_asset(action="save", asset_path=...)` after the batch |
| Editing UVs on a skeletal mesh | Not supported — only StaticMesh works |
| Setting `LightMapCoordinateIndex` past the channel count | Use `set_uv_channel_count` first, or `add_uv_channel` |

---

## Return Types — Exact Properties

### FUVMappingResult (every mutating call)
| Property | Type | Description |
|---|---|---|
| `success` | bool | True iff the operation completed |
| `mesh_path` | str | Echoes the input path |
| `message` | str | Failure reason or success summary |

### FUVChannelInfo (`list_uv_channels`, `get_uv_channel_info`)
| Property | Type | Description |
|---|---|---|
| `lod_index` | int | LOD this channel belongs to |
| `channel_index` | int | UV channel (0..7) |
| `vertex_instance_count` | int | Number of vertex instances |
| `triangle_count` | int | Number of triangles |
| `min_u`, `min_v`, `max_u`, `max_v` | float | UV bounding box |
| `overlap_percent` | float | % of triangles with overlapping UV AABB. -1 if mesh too large (>5000 tris) |
| `in_unit_square_percent` | float | % of vertex UVs inside `[0,1]^2` |
| `texel_density1k` | float | Avg texels/UE-unit at 1024px reference |

### FUVHealthReport (`get_uv_health`)
| Property | Type | Description |
|---|---|---|
| `mesh_path` | str | |
| `lod_count` | int | |
| `lightmap_coordinate_index` | int | The channel lightmaps sample at runtime |
| `light_map_resolution` | int | Per-mesh lightmap texel size |
| `b_generate_lightmap_uvs` | bool | LOD 0 BuildSettings flag |
| `channels` | list[FUVChannelInfo] | Per-LOD per-channel stats |
| `b_lightmap_has_overlaps` | bool | True if the lightmap channel has any AABB overlap |
| `warnings` | list[str] | Human-readable warnings |

### FUVLightmapSettings (`get_lightmap_settings`)
| Property | Type | Description |
|---|---|---|
| `b_generate_lightmap_uvs` | bool | LOD 0 BuildSettings flag |
| `source_lightmap_index` | int | UV channel the generator reads seams from |
| `destination_lightmap_index` | int | UV channel the generator writes into |
| `lightmap_coordinate_index` | int | Runtime sample channel |
| `light_map_resolution` | int | Mesh-level lightmap resolution |
| `min_lightmap_resolution` | int | Min allowed lightmap resolution |

---

## Workflows

### Workflow: Generate Lightmap UVs for a Newly Imported Mesh

```python
import unreal

mesh = "/Game/Meshes/SM_Wall"

# 1. Health check
ok, health = unreal.UVMappingService.get_uv_health(mesh)
print(f"Channels found: {len(health.channels)}, lightmap idx: {health.lightmap_coordinate_index}")

# 2. Generate the lightmap channel (creates channel 1 if needed, sets LightMapCoordinateIndex=1)
result = unreal.UVMappingService.generate_lightmap_uvs(
    mesh,
    source_uv_index=0,
    dest_uv_index=1,
    min_chart_spacing_percent=1.0,
)
print(f"GENERATED: {result.message}")

# 3. Configure lightmap resolution (256 for hero props, 64 for background)
unreal.UVMappingService.set_lightmap_settings(mesh, 1, 0, 128, True)

# 4. Save
manage_asset(action="save", asset_path=mesh)

# 5. Verify
ok, health = unreal.UVMappingService.get_uv_health(mesh)
assert not health.b_lightmap_has_overlaps, "Lightmap still has overlaps after generation"
```

### Workflow: Add a Detail-Texture UV Channel and Tile It

```python
import unreal

mesh = "/Game/Meshes/SM_Wall"

# Add channel 2 if not present
if not unreal.UVMappingService.mesh_has_uv_channel(mesh, 0, 2):
    r = unreal.UVMappingService.add_uv_channel(mesh, 0)
    print(f"ADDED: {r.message}")

# Copy material UVs (channel 0) into channel 2 as a starting point
r = unreal.UVMappingService.copy_uv_channel(mesh, 0, 0, 2)
print(f"COPIED: {r.message}")

# Tile by 4 (scale UVs)
r = unreal.UVMappingService.transform_uvs(mesh, 0, 2, scale_u=4.0, scale_v=4.0)
print(f"TILED: {r.message}")

manage_asset(action="save", asset_path=mesh)
```

### Workflow: Auto-Unwrap a Mesh That Imported with Bad UVs

```python
import unreal

mesh = "/Game/Meshes/SM_RuinPiece"

# Box projection produces clean per-face charts on chunky props
r = unreal.UVMappingService.auto_unwrap_uvs(
    mesh, lod_index=0, channel_index=0,
    projection_type="Box",
    hard_angle_threshold=66.0,
)
print(f"UNWRAPPED: {r.message}")

# Pack the result tightly
r = unreal.UVMappingService.pack_uvs(mesh, 0, 0, padding_percent=1.0)
print(f"PACKED: {r.message}")

# Regenerate the lightmap channel from the new charts
unreal.UVMappingService.generate_lightmap_uvs(mesh, 0, 1, 1.0)

manage_asset(action="save", asset_path=mesh)
```

### Workflow: Visual UV Inspection (AI-Friendly)

```python
import unreal, os

mesh = "/Game/Meshes/SM_Wall"
out = os.path.join(os.environ["TEMP"], "uv_layout.png")

r = unreal.UVMappingService.export_uv_layout_image(
    mesh, lod_index=0, channel_index=1, output_path=out, image_size=1024)
print(f"EXPORTED: {r.message}")
# Then attach the PNG with the screenshots skill / attach_image
```

### Workflow: Batch Lightmap Pass on All Meshes in a Folder

```python
import unreal

results = manage_asset(action="list", path="/Game/Meshes/Architecture")
for asset in results["assets"]:
    if asset["asset_class"] != "StaticMesh":
        continue
    path = asset["asset_path"]

    ok, health = unreal.UVMappingService.get_uv_health(path)
    if health.b_lightmap_has_overlaps or not health.b_generate_lightmap_uvs:
        unreal.UVMappingService.generate_lightmap_uvs(path, 0, 1, 1.0)
        unreal.UVMappingService.set_lightmap_settings(path, 1, 0, 128, True)
        manage_asset(action="save", asset_path=path)
        print(f"FIXED: {path}")
    else:
        print(f"OK:    {path}")
```

---

## Property Formats

### Projection Types (`auto_unwrap_uvs`)
String, case-insensitive:
- `"Planar"` — projects along world +Z. Best for flat panels.
- `"Box"` — six-axis projection with seams along sharp edges. Best general-purpose unwrap.
- `"Cylindrical"` — wraps around world Z axis. Best for tubes / columns.

### Min Chart Spacing (`generate_lightmap_uvs`)
Float, percent of UV space (0.5..2.0 typical):
- `0.5` — tight packing, risk of bleeding at low lightmap resolution
- `1.0` — default, safe for 64–128 lightmap resolution
- `2.0` — generous, good for 32 resolution background props

### Lightmap Resolution (`set_lightmap_settings`)
Power of two preferred:
- `32`–`64` — background / non-hero
- `128` — typical world geometry
- `256`–`512` — hero props, large architecture

---

## Verification After Edits

For lightmap-related changes, **always re-run `get_uv_health`** after the save to verify:

```python
ok, health = unreal.UVMappingService.get_uv_health(mesh)
assert ok
assert not health.b_lightmap_has_overlaps
assert health.lightmap_coordinate_index == 1
```

For unwrap operations, export the layout PNG and visually inspect it via `attach_image` — the cheapest way to confirm the unwrap looks reasonable.
