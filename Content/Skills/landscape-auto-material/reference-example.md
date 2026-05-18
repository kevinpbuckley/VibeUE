---
name: reference-example
description: Concrete reference architecture for a production landscape master material — full function chain, biome instances, and texture naming convention.
---

## Reference Architecture Example

A production auto-material setup typically looks like this:

```
M_Landscape_Master (Master Material)
├── MF_AutoLayer ─────── Drives automatic layer selection
│   ├── MF_Altitude_Blend ── Height-based layer transitions
│   └── MF_Slope_Blend ───── Slope-based layer transitions
├── MF_Layer_Grass ──────── Per-layer texture sampling
├── MF_Layer_Rock ───────── Per-layer texture sampling
├── MF_Layer_Snow ───────── Per-layer texture sampling
├── MF_Layer_Dirt / Forest / Beach / Desert / Grass_Dry / Rock_Desert
├── MF_RVT ─────────────── Runtime Virtual Texture output
├── MF_Distance_Blends ─── LOD transitions
├── MF_Distance_Fades ──── Far-distance fading
├── MF_BumpOffset_WorldAlignedNormal ── Parallax
├── MF_Color_Correction ── Color grading
├── MF_Color_Variations ── Per-instance color variation
├── MF_Normal_Correction ─ Normal map fixes
├── MF_PBR_Conversion ──── Roughness/metallic conversion
├── MF_Texture_Scale ───── UV tiling control
├── MF_Displacement ────── World position offset
└── MF_Wind_System ─────── Foliage wind animation
```

**Biome instances** override textures and blend thresholds:
- `MI_Landscape_Default_01` — grasslands with moderate slopes
- `MI_Landscape_Island_01` — island variant with beach/sand
- `MI_Landscape_Mountain_01` — alpine with more snow/rock

**Texture naming convention:**
| Suffix | Meaning | Example |
|--------|---------|---------|
| `_BC` | Base Color / Albedo | `T_Ground_Grass_01_BC` |
| `_N` | Normal Map | `T_Ground_Grass_01_N` |
| `_H` | Height Map | `T_Ground_Grass_01_H` |
| `_RMA` | Roughness-Metallic-AO packed | `T_Nature_Default_RMA_01` |
