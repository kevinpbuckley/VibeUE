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
