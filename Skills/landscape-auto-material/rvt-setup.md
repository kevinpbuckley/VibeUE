---
name: rvt-setup
description: Runtime Virtual Texture material types, sizing guidelines, inspection code, and common RVT issues with causes and fixes.
---

## RVT Setup Guide

### RVT Material Types

| Type | Channels | Use Case |
|------|----------|----------|
| `BaseColor` | RGB | Color-only caching (simplest) |
| `BaseColor_Normal_Roughness` | RGB + Normal + Roughness | Standard landscape (most common) |
| `BaseColor_Normal_Specular` | RGB + Normal + Specular | For specular-heavy materials |
| `WorldHeight` | Height only | For distance-based effects, atmosphere |

**The RVT material type must match what your material actually outputs.**

### RVT Sizing Guidelines

| Landscape Size | TileCount | TileSize | Total Resolution |
|----------------|-----------|----------|-----------------|
| Small (1-4 km²) | 128 | 256 | 32K × 32K |
| Medium (4-16 km²) | 256 | 256 | 64K × 64K |
| Large (16+ km²) | 512 | 256 | 128K × 128K |

**Notes:**
- Higher tile count = more memory but better streaming
- `bContinuousUpdate = true` re-renders every frame (expensive, only for dynamic materials)
- `bSinglePhysicalSpace = true` disables streaming (entire VT in memory, fast but memory-heavy)

### Inspecting Existing RVTs

```python
import unreal

info = unreal.RuntimeVirtualTextureService.get_runtime_virtual_texture_info(
    "/Game/VirtualTextures/RVT_Landscape_01")

print(f"Type: {info.material_type}")
print(f"Tiles: {info.tile_count} × {info.tile_size}px")
print(f"Border: {info.tile_border_size}")
print(f"Continuous: {info.continuous_update}")
print(f"Single space: {info.single_physical_space}")
```

### Common RVT Issues

| Issue | Cause | Fix |
|-------|-------|-----|
| RVT Shows Black | Missing `bUsedWithVirtualTexturing = true` | Set property on material |
| RVT Shows Black | RVT output node not connected | Connect matching pins |
| RVT Shows Black | No RVT Volume actor in level | Create volume via `create_rvt_volume` |
| RVT Shows Blurry | TileCount/TileSize too low | Increase values |
| RVT Shows Blurry | Volume bounds don't match landscape | Recreate volume |
| Performance Regression | `bContinuousUpdate` enabled unnecessarily | Disable unless material changes per-frame |
| Performance Regression | Volume much larger than landscape | Resize to match |
