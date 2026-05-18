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
