# Biome Configuration Guide

## Overview

A **biome** is a specific landscape look defined entirely through a **material instance** of the master material. The master material's graph stays unchanged — biomes differ only in parameter values (textures, blend thresholds, color corrections).

## Creating a New Biome

### Step 1: Create Material Instance

```python
import unreal

inst = unreal.MaterialService.create_material_instance(
    "MI_Landscape_<BiomeName>",
    "/Game/Materials/Landscape",
    "/Game/Materials/M_Landscape_Master"  # Parent master material
)
inst_path = inst.asset_path
```

### Step 2: Configure Textures

Each layer's textures are exposed as **Texture Parameters** in the master:

```python
import unreal

inst_path = "/Game/Materials/Landscape/MI_Landscape_Tropical"

count = unreal.MaterialService.set_instance_parameters_bulk(
    inst_path,
    # Names
    ["Layer01_BaseColor", "Layer01_Normal", "Layer01_Roughness",
     "Layer02_BaseColor", "Layer02_Normal", "Layer02_Roughness",
     "Layer03_BaseColor", "Layer03_Normal", "Layer03_Roughness"],
    # Types
    ["Texture", "Texture", "Texture",
     "Texture", "Texture", "Texture",
     "Texture", "Texture", "Texture"],
    # Values
    ["/Game/Textures/T_Tropical_Grass_BC", "/Game/Textures/T_Tropical_Grass_N", "/Game/Textures/T_Tropical_Grass_RMA",
     "/Game/Textures/T_Tropical_Rock_BC", "/Game/Textures/T_Tropical_Rock_N", "/Game/Textures/T_Tropical_Rock_RMA",
     "/Game/Textures/T_Tropical_Sand_BC", "/Game/Textures/T_Tropical_Sand_N", "/Game/Textures/T_Tropical_Sand_RMA"]
)
```

### Step 3: Configure Blend Thresholds

```python
count = unreal.MaterialService.set_instance_parameters_bulk(
    inst_path,
    ["SlopeThreshold", "SlopeBlendWidth", "AltitudeThreshold", "AltitudeBlendHeight"],
    ["Scalar", "Scalar", "Scalar", "Scalar"],
    ["0.65", "0.2", "3000.0", "800.0"]
)
```

### Step 4: Configure Visual Adjustments

```python
count = unreal.MaterialService.set_instance_parameters_bulk(
    inst_path,
    ["ColorSaturation", "ColorBrightness", "NormalIntensity", "RoughnessScale",
     "UVTiling_Layer01", "UVTiling_Layer02", "UVTiling_Layer03"],
    ["Scalar", "Scalar", "Scalar", "Scalar",
     "Scalar", "Scalar", "Scalar"],
    ["1.1", "1.05", "1.0", "0.95",
     "0.01", "0.008", "0.012"]
)
```

### Step 5: Toggle Features via Static Switches

```python
count = unreal.MaterialService.set_instance_parameters_bulk(
    inst_path,
    ["EnableSnow", "EnableDisplacement", "EnableRVT", "EnableDistanceFades"],
    ["StaticSwitch", "StaticSwitch", "StaticSwitch", "StaticSwitch"],
    ["false", "true", "true", "true"]
)
```

## Biome Comparison Table

Example biome configurations from Real_Landscape:

| Parameter | Default | Meadow Island | Meadow Mountain |
|-----------|---------|--------------|-----------------|
| SlopeThreshold | 0.7 | 0.75 | 0.6 |
| AltitudeThreshold | 5000 | 500 | 8000 |
| EnableSnow | true | false | true |
| Layer01 (base) | Grass | Tropical Grass | Alpine Grass |
| Layer02 (slope) | Rock | Sandstone | Granite |
| Layer03 (height) | Snow | Beach Sand | Snow |
| ColorSaturation | 1.0 | 1.2 | 0.9 |
| UVTiling | 0.01 | 0.01 | 0.008 |

## Biome Workflow Tips

### Use FindLandscapeTextures for Discovery

Before creating a biome, scan available textures:

```python
textures = unreal.LandscapeMaterialService.find_landscape_textures("/Game/Textures")
for t in textures:
    print(f"{t.terrain_type}: {t.albedo_path}")
```

### Test Biome Switching

Assign different instances to see biome changes live:
```python
# Switch landscape to tropical biome
unreal.LandscapeMaterialService.assign_material_to_landscape(
    "MyLandscape",
    "/Game/Materials/Landscape/MI_Landscape_Tropical",
    layer_info_map  # Same layer infos work across biomes
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
