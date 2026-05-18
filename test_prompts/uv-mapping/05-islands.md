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
