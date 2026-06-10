# UV Mapping Tests — Inspection

Read-only checks: list channels, get info per channel, full health report.
None of these prompts modify the mesh.

---

### 1. List every UV channel on every LOD

```
Show me every UV channel on every LOD of /Game/Meshes/SM_Cube_Big_Brick,
with vertex-instance count, triangle count, UV bounds, and texel density.
```

---

### 2. Quick health report

```
Run a UV health report on /Game/Meshes/SM_Disc_Wide_Brick_v3 and tell me:
- How many LODs it has
- Lightmap coordinate index and resolution
- Whether the lightmap channel has any overlapping triangles
- Any warnings the engine flagged
```

---

### 3. One channel, one LOD

```
What's the texel density of LOD 0 channel 0 on
/Game/Meshes/SM_Sphere_Huge_Soil at a 1024-pixel reference texture size?
Return min/max UV bounds too.
```

---

### 4. Existence check before editing

```
Before I try to edit channel 3 on /Game/Meshes/SM_Pillar_Tall_Brick,
confirm that channel even exists. If not, tell me how many channels it
currently has.
```

---

### 5. Identify overlapping lightmap UVs

```
Which mesh in /Game/Meshes has overlapping UVs on its lightmap channel?
Run get_uv_health on each one and report any that flag overlaps.
```

---

### 6. Polygon-group inventory

```
What polygon groups (material slots) does /Game/Meshes/SM_Cube_Big_Brick
expose on LOD 0?
```


---

# UV Mapping Tests — Channel Lifecycle

Add, remove, copy, and resize the UV channel array on a static mesh.

---

### 1. Add a fresh UV channel

```
Add a new (empty) UV channel to LOD 0 of
/Game/Meshes/SM_Cube_Big_Brick. Save. What index did it get?
```

---

### 2. Remove a UV channel

```
Remove UV channel 2 from LOD 0 of /Game/Meshes/SM_Cube_Big_Brick. Save.
Confirm the lightmap coordinate index was adjusted if needed.
```

---

### 3. Channel 0 cannot be removed

```
Try to remove UV channel 0 from /Game/Meshes/SM_Cube_Big_Brick.
This should fail with a clear error message — verify the failure reason
explicitly says channel 0 is protected.
```

---

### 4. Copy channel 0 into channel 2 (auto-grow)

```
On LOD 0 of /Game/Meshes/SM_Cube_Big_Brick, copy UV channel 0 into channel 2.
The channel array should auto-grow to fit if 2 doesn't exist yet. Save.
```

---

### 5. Resize the channel array

```
Set /Game/Meshes/SM_Cube_Big_Brick LOD 0 to have exactly 4 UV channels.
Save. If the lightmap was at index 3 and we shrink, the lightmap index
should clamp.
```

---

### 6. Reject channel count out of range

```
Try to set LOD 0 of /Game/Meshes/SM_Cube_Big_Brick to 9 UV channels —
which is over the engine's MAX_MESH_TEXTURE_COORDS_MD limit of 8. The
operation should fail with a clear error.
```


---

# UV Mapping Tests — Generation

Lightmap UV generation, auto-unwrap projections, and packing.

---

### 1. Generate a lightmap UV channel

```
On /Game/Meshes/SM_Cube_Big_Brick, generate a lightmap UV channel into
channel 1, sourcing seam information from channel 0. Use a min chart
spacing of 1.0 percent. Save.
After: confirm `LightMapCoordinateIndex` was set to 1 automatically.
```

---

### 2. Auto-unwrap with planar projection

```
Wipe LOD 0 channel 0 of /Game/Meshes/SM_Slab_Wide_Soil and replace it with
a planar projection from above (world +Z). Save. Show the new UV bounds.
```

---

### 3. Auto-unwrap with box projection

```
Wipe LOD 0 channel 0 of /Game/Meshes/SM_Cube_Big_Brick and replace it with
a Box projection (per-face axis-aligned). Save.
```

---

### 4. Auto-unwrap with cylindrical projection

```
Wipe LOD 0 channel 0 of /Game/Meshes/SM_Pillar_Tall_Brick and replace it
with a Cylindrical projection wrapping around world Z. Save. The natural
UV bounds should approach (0,0)..(1,1).
```

---

### 5. Reject invalid projection name

```
Try to auto-unwrap /Game/Meshes/SM_Cube_Big_Brick using a projection type
called "Spherical". This isn't supported — the call should fail with a
message listing the valid options (Planar, Box, Cylindrical).
```

---

### 6. Pack existing UVs to fit unit square

```
The mesh /Game/Meshes/SM_Cube_Big_Brick channel 0 has UVs spanning beyond
[0,1] (e.g., from a previous transform). Repack them tightly into the
unit square with 1% padding. Save.
```


---

# UV Mapping Tests — Transforms

Whole-channel and per-region UV transforms.

---

### 1. Scale a channel uniformly

```
Scale UV channel 0 of /Game/Meshes/SM_Cube_Big_Brick by 4× in both U and V
to get tighter brick tiling. Save and confirm new bounds are roughly
(0,0)..(4,4).
```

---

### 2. Rotate a channel

```
Rotate UV channel 0 of /Game/Meshes/SM_Slab_Wide_Soil by 45 degrees.
Save.
```

---

### 3. Translate a channel

```
Translate UV channel 0 of /Game/Meshes/SM_Sphere_Med_Soil by (0.25, 0.0).
Save and verify min U shifted by 0.25.
```

---

### 4. Flip UVs

```
Flip UV channel 0 of /Game/Meshes/SM_Cube_Big_Brick along U only.
Save. Verify by inspecting any vertex's UV — it should now be (1-U, V)
of its original.
```

---

### 5. No-op flip is harmless

```
Call flip on /Game/Meshes/SM_Cube_Big_Brick with both U=false and V=false.
This should succeed with a "No flip requested" message and not modify the
mesh.
```

---

### 6. Per-region transform: side strip only

```
On /Game/Meshes/SM_Disc_Wide_Brick_v3, scale UV U by 4× ONLY for vertex
instances whose normal is roughly horizontal (the side strip of the disc) —
caps must stay untouched. Use axis (0,0,1) and a dot window of [-0.5, 0.5].
Verify with a count-by-normal preview before applying.
```

---

### 7. Per-region transform: top cap only

```
On /Game/Meshes/SM_Pillar_Tall_Brick, double UV V on just the top cap
(normal ≈ +Z, dot window [0.7, 1.0]). Bottom cap and side untouched.
```

---

### 8. Per-polygon-group transform

```
The mesh /Game/Meshes/SM_Wall has two polygon groups: "Body" and "Trim".
Scale only the "Trim" group's UVs by 2× while leaving "Body" alone. List
groups first to discover the names.
```

---

### 9. Filter that selects nothing

```
Try to transform_uvs_by_normal on /Game/Meshes/SM_Cube_Big_Brick with axis
(0,0,1) and dot window [0.99, 1.001]. On a cube this should select only
vertices facing nearly straight up. If no vertex matches the window, the
call should report "No vertex instances matched" and not modify anything.
```


---

# UV Mapping Tests — UV Islands

Detect connected UV islands and operate on them by id.

---

### 1. Detect islands on a cylinder

```
Identify the UV islands on LOD 0 channel 0 of
/Game/Meshes/SM_Pillar_Tall_Brick. A cylinder should expose three islands:
top cap, bottom cap, and the side strip. Show id, triangle count, average
normal, world center, and UV bounds for each.
```

---

### 2. Detect islands on a cube

```
Identify the UV islands of /Game/Meshes/SM_Cube_Big_Brick LOD 0 channel 0.
On a cube with stock UVs you'll typically get 6 islands (one per face).
Confirm the count and that each island's average normal points along a
single world axis.
```

---

### 3. Pick the largest island and transform it

```
On /Game/Meshes/SM_Disc_Wide_Brick_v3, identify all UV islands and apply a
4× U scale to whichever island has the most triangles. The other islands
must remain untouched.
```

---

### 4. Pick an island by world position

```
On /Game/Meshes/SM_Pillar_Tall_Brick, find the UV island whose world center
has the highest Z (the top cap), and rotate its UVs by 90 degrees. Save.
```

---

### 5. Out-of-range island id

```
On /Game/Meshes/SM_Cube_Big_Brick, try to apply a transform to island id
99999. The call should fail with a clear "out of range" error reporting
how many islands actually exist.
```

---

### 6. Stable ids across calls

```
Call identify_uv_islands twice in a row on /Game/Meshes/SM_Cube_Big_Brick
without modifying the mesh in between. The returned island ids should be
identical between the two calls — verify by comparing.
```


---

# UV Mapping Tests — Lightmap Settings & Layout Export

Configure lightmap-related fields on a static mesh, and render UV layouts
to disk for visual inspection.

---

### 1. Read current lightmap settings

```
Show the lightmap settings on /Game/Meshes/SM_Cube_Big_Brick:
- bGenerateLightmapUVs flag
- Source/Destination indices
- LightMapCoordinateIndex
- LightMapResolution
- MinLightmapResolution
```

---

### 2. Set a complete lightmap configuration

```
Configure /Game/Meshes/SM_Cube_Big_Brick:
- LightMapCoordinateIndex = 1
- SourceLightmapIndex = 0
- LightMapResolution = 128
- bGenerateLightmapUVs = true
Save. The mesh should rebuild with regenerated lightmap UVs.
```

---

### 3. Bump a single lightmap field

```
Just raise the LightMapResolution on /Game/Meshes/SM_Cube_Big_Brick from
its current value to 256, leave everything else as-is. Save.
```

---

### 4. Export UV layout (channel 0)

```
Render the LOD 0 channel 0 UV layout of /Game/Meshes/SM_Cube_Big_Brick to
a 1024×1024 PNG named "cube_ch0.png" in the project's Saved/VibeUE/Screenshots
folder. Then attach the PNG so I can review the layout.
```

---

### 5. Export auto-fits non-unit-square UVs

```
After scaling channel 0 of /Game/Meshes/SM_Cube_Big_Brick by 6× (so UVs
span 0..6), export the layout. Verify the exporter auto-fits the bounds —
the [0,1] reference frame should be visible inside the larger view, not
clipped.
```

---

### 6. Export the lightmap channel

```
Render the LOD 0 channel 1 (lightmap) layout of /Game/Meshes/SM_Cube_Big_Brick
to a 2048×2048 PNG. After generate_lightmap_uvs runs, this should show
nicely packed islands.
```

---

### 7. Export with bad channel index

```
Try to export channel 7 on /Game/Meshes/SM_Cube_Big_Brick when only 2
channels exist. The call should fail with a "channel out of range"
message — no PNG written.
```


---

# UV Mapping Tests — End-to-End Workflows

Compositions that exercise multiple methods in realistic sequences.

---

### 1. Lightmap pass on a freshly imported mesh

```
For /Game/Meshes/SM_Newly_Imported, do the full lightmap setup:
1. Get current health and confirm lightmap channel is missing or overlapping.
2. Generate the lightmap UV channel from channel 0 into channel 1
   with 1% chart spacing.
3. Set LightMapResolution to 128.
4. Re-run health and confirm bLightmapHasOverlaps is now false.
5. Save the asset.
```

---

### 2. Fix a thin disc's side aspect (the disc workflow)

```
The mesh /Game/Meshes/SM_Disc_Wide_Brick_v3 is a cylinder duplicate scaled
in actor world to 250 cm wide × 40 cm tall, so its side strip has a 20:1
aspect that turns brick textures into pinstripes. Compensate by scaling
the side strip's UVs in U by 4× — caps untouched.

Steps:
1. Identify which vertex instances are on the side via count_vertices_by_normal
   with axis (0,0,1), dot window [-0.5, 0.5]. Should return ~960.
2. transform_uvs_by_normal with the same window, scale_u=4, scale_v=1.
3. Save.
4. Visual check: export the channel-0 layout to a PNG.
```

---

### 3. Detail-tile UV channel on top of base UVs

```
Add a second material UV channel to /Game/Meshes/SM_Wall for detail-tile
sampling:
1. add_uv_channel on LOD 0 (channel 2 should be created).
2. copy_uv_channel from 0 into 2.
3. transform_uvs to scale channel 2 by 4× in both axes for tighter detail.
4. Save and re-inspect — channel 0 unchanged, channel 2 is 4× tiled.
```

---

### 4. Auto-unwrap rescue + repack

```
The mesh /Game/Meshes/SM_RuinPiece imported with garbage UVs. Rescue:
1. auto_unwrap_uvs with Box projection on channel 0.
2. pack_uvs to tighten the islands into the unit square with 1% padding.
3. generate_lightmap_uvs from the new channel 0 into channel 1.
4. Set LightMapCoordinateIndex = 1 and LightMapResolution = 64.
5. Save.
```

---

### 5. Batch lightmap pass on every mesh in a folder

```
For every StaticMesh under /Game/Meshes, run a quick health check.
For any mesh whose bLightmapHasOverlaps is true OR whose
bGenerateLightmapUVs is false, fix it:
- generate_lightmap_uvs (0 → 1, spacing 1%)
- set_lightmap_settings (coord=1, source=0, res=128, generate=true)
- save.
At the end, report which meshes were fixed vs which were already clean.
```

---

### 6. Per-island artistic edit

```
On /Game/Meshes/SM_Pillar_Tall_Brick:
1. Identify all UV islands on channel 0.
2. Find the side-strip island (the one with average normal in the XY plane).
3. Scale that one island's U by 6× to get more brick repeats around the
   pillar — caps and other islands untouched.
4. Save.
5. Export a layout PNG so we can confirm only the side strip changed.
```

