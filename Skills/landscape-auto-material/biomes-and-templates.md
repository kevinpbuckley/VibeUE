---
name: biomes-and-templates
description: Biome configuration guide (comparison table, static-switch toggles, naming) and the standard layer function template (inputs, outputs, catalog).
---

## Biome Configuration Guide

A **biome** is a specific landscape look defined entirely through a **material instance** of the master material. The master graph stays unchanged — biomes differ only in parameter values.

### Biome Comparison Table

| Parameter | Default | Island Variant | Mountain Variant |
|-----------|---------|--------------|-----------------|
| SlopeThreshold | 0.7 | 0.75 | 0.6 |
| AltitudeThreshold | 5000 | 500 | 8000 |
| EnableSnow | true | false | true |
| Layer01 (base) | Grass | Tropical Grass | Alpine Grass |
| Layer02 (slope) | Rock | Sandstone | Granite |
| Layer03 (height) | Snow | Beach Sand | Snow |
| ColorSaturation | 1.0 | 1.2 | 0.9 |
| UVTiling | 0.01 | 0.01 | 0.008 |

### Configure Feature Toggles via Static Switches

```python
count = unreal.MaterialService.set_instance_parameters_bulk(
    inst_path,
    ["EnableSnow", "EnableDisplacement", "EnableRVT", "EnableDistanceFades"],
    ["StaticSwitch", "StaticSwitch", "StaticSwitch", "StaticSwitch"],
    ["false", "true", "true", "true"]
)
```

### Biome Naming Convention

```
MI_Landscape_<Region>_<Variant>_##
```
Examples:
- `MI_Landscape_Default_01` — generic grassland
- `MI_Landscape_Tropical_Island_01` — tropical island variant
- `MI_Landscape_Alpine_Valley_01` — alpine valley
- `MI_Landscape_Desert_Canyon_01` — desert canyon

---

## Layer Function Template

A layer function encapsulates all texture sampling logic for a single terrain type. This makes per-terrain materials modular and reusable.

### Standard Interface

**Standard inputs** (every layer function should have these):

```python
# Texture inputs
unreal.MaterialNodeService.add_function_input(
    func_path, "BaseColorTexture", "Texture2D", 0,
    "Albedo/diffuse texture for this terrain type")
unreal.MaterialNodeService.add_function_input(
    func_path, "NormalTexture", "Texture2D", 1,
    "Normal map texture")
unreal.MaterialNodeService.add_function_input(
    func_path, "RoughnessTexture", "Texture2D", 2,
    "Roughness texture (or RMA packed)")

# Tiling control
unreal.MaterialNodeService.add_function_input(
    func_path, "UVTiling", "Scalar", 3,
    "UV tiling scale (default 0.01 for landscapes)")

# Optional: detail textures for close-up
unreal.MaterialNodeService.add_function_input(
    func_path, "DetailBaseColor", "Texture2D", 10,
    "Close-up detail texture (optional)")
unreal.MaterialNodeService.add_function_input(
    func_path, "DetailScale", "Scalar", 11,
    "Detail texture tiling multiplier")
```

**Standard outputs:**

```python
unreal.MaterialNodeService.add_function_output(
    func_path, "BaseColor", 0, "Sampled albedo (RGB)")
unreal.MaterialNodeService.add_function_output(
    func_path, "Normal", 1, "Sampled normal (RGB)")
unreal.MaterialNodeService.add_function_output(
    func_path, "Roughness", 2, "Sampled roughness (Scalar)")
```

### Internal Graph Pattern

```
LandscapeLayerCoords (UV tiling)
    ↓
TextureSample (BaseColor) → Output: BaseColor
TextureSample (Normal)    → Output: Normal
TextureSample (Roughness) → Output: Roughness
    ↓ (optional)
DetailTextureSample → Lerp with base by distance → Final outputs
```

### Layer Function Catalog

| Function | Terrain | Textures | Special Features |
|----------|---------|----------|-----------------|
| `MF_Layer_Grass` | Grassland | BC + N + H | Color variation |
| `MF_Layer_Rock` | Rocky surfaces | BC + N + H | World-aligned UV |
| `MF_Layer_Snow` | Snow/ice | BC + N | Sparkle overlay |
| `MF_Layer_Dirt` | Bare earth | BC + N + H | Moisture darkening |
| `MF_Layer_Forest` | Forest floor | BC + N + H | Leaf litter blend |
| `MF_Layer_Beach` | Sand/beach | BC + N | Wet/dry transition |
| `MF_Layer_Desert` | Desert sand | BC + N + H | Wind ripple normal |
| `MF_Layer_Grass_Dry` | Dry grass | BC + N | Seasonal variant |
| `MF_Layer_Rock_Desert` | Desert rock | BC + N + H | Eroded variant |

### Layer Function Tips

- **Keep functions focused**: One terrain type per function, one concern per function
- **Use consistent pin naming**: `BaseColor`, `Normal`, `Roughness` for all output functions
- **Sort priorities matter**: Keep consistent across all layer functions (0=BC, 1=Normal, 2=Roughness, 3=UV)
- **Save before referencing**: Always `save_asset()` the function before creating a `MaterialExpressionMaterialFunctionCall` to it
