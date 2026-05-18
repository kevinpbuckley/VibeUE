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
