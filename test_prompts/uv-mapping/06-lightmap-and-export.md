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
