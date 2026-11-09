---
name: material-functions
description: Patterns for common landscape material functions (Slope_Blend, Altitude_Blend, Layer_*, RVT) and the EFunctionInputType reference table.
---

## Material Function Patterns

### MF_Slope_Blend — Slope Detection

Generates a 0-1 mask from the landscape surface normal angle:
- **Input**: World Normal (Vector3), Slope Threshold (Scalar), Blend Width (Scalar)
- **Output**: Slope Mask (Scalar) — 0 = flat, 1 = steep
- **Internal**: `dot(WorldNormal, UpVector)` → remap with threshold/width

### MF_Altitude_Blend — Height Detection

Generates a 0-1 mask from world position Z:
- **Input**: World Position (Vector3), Height Threshold (Scalar), Blend Height (Scalar)
- **Output**: Height Mask (Scalar) — 0 = below threshold, 1 = above
- **Internal**: `WorldPosition.Z` → smoothstep with threshold/blend

### MF_Layer_* — Per-Layer Texture Sampling

Each layer function encapsulates sampling for one terrain type:
- **Input**: BaseColor Texture (Texture2D), Normal Texture (Texture2D), UV Tiling (Scalar)
- **Output**: Sampled Color (Vector3), Sampled Normal (Vector3), Sampled Roughness (Scalar)
- **Internal**: TextureSample + UV scaling + optional detail blending

### MF_RVT — Runtime Virtual Texture Output

Wraps layer blend results into RVT output pins:
- **Input**: BaseColor (Vector3), Normal (Vector3), Roughness (Scalar)
- **Output**: (connects to `MaterialExpressionRuntimeVirtualTextureOutput` internally)

---

## Material Function Input Types

| Type | EFunctionInputType | Description |
|------|---------------------|-------------|
| `Scalar` | 0 | Single float value |
| `Vector2` | 1 | 2D vector (UV coords) |
| `Vector3` | 2 | 3D vector (color, normal, position) |
| `Vector4` | 3 | 4D vector (RGBA) |
| `Texture2D` | 4 | 2D texture reference |
| `TextureCube` | 5 | Cubemap texture |
| `TextureExternal` | 6 | External texture |
| `VolumeTexture` | 7 | 3D texture |
| `StaticBool` | 8 | Static switch (compile-time) |
| `MaterialAttributes` | 9 | Full material attributes struct |
