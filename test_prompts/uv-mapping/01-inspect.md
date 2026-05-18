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
