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
