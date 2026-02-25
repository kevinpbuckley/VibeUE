# Auto-Layer Architecture

## Overview

The auto-layer system uses a chain of material functions to automatically select and blend landscape layers based on terrain properties (slope angle, altitude, distance from camera). This eliminates manual layer painting for the base coverage, while still allowing artist paint overrides.

## Function Chain

```
MF_AutoLayer_01 (orchestrator)
│
├── Reads: World Position (from VertexNormalWS / AbsoluteWorldPosition)
├── Reads: World Normal (from VertexNormalWS)
│
├── Calls: MF_Altitude_Blend_01
│   ├── Input: WorldPosition.Z
│   ├── Parameter: Height Threshold (e.g., 5000 units)
│   ├── Parameter: Blend Height (transition width, e.g., 500 units)
│   └── Output: Height Mask (0.0 = below, 1.0 = above)
│
├── Calls: MF_Slope_Blend_01
│   ├── Input: World Normal
│   ├── Parameter: Slope Threshold (0.0-1.0, where 0.7 ≈ 45°)
│   ├── Parameter: Slope Blend Width (e.g., 0.15)
│   └── Output: Slope Mask (0.0 = flat, 1.0 = steep)
│
├── Combines masks:
│   ├── SlopeMask → Rock layer weight
│   ├── HeightMask → Snow layer weight
│   ├── (1 - SlopeMask) * (1 - HeightMask) → Base layer weight (Grass)
│   └── Optional: SlopeMask * HeightMask → Alpine Rock variant
│
└── Per-layer sampling:
    ├── MF_Layer_Grass(weight=base) → Sampled BaseColor, Normal, Roughness
    ├── MF_Layer_Rock(weight=slope) → Sampled BaseColor, Normal, Roughness
    ├── MF_Layer_Snow(weight=height) → Sampled BaseColor, Normal, Roughness
    └── Lerp/Blend all layers by weights → Final output
```

## How Masks Combine

The key insight is that auto-layer uses **multiplicative mask composition**:

```
base_weight  = (1.0 - slope_mask) * (1.0 - height_mask)
slope_weight = slope_mask * (1.0 - height_mask)  // Rock on slopes but not peaks
height_weight = height_mask                        // Snow on peaks regardless of slope
```

This ensures weights always sum to ~1.0 without explicit normalization.

## Height Blend vs. Weight Blend

- **Auto-layer weights** (from slope/altitude) use `LB_HeightBlend` for smooth transitions
- **Painted layers** use `LB_WeightBlend` for artist control
- When both exist on the same landscape, painted layers **override** auto-layer in painted regions

## Parameter Flow Through Instances

All thresholds are exposed as **Scalar Parameters** in the master material:
- `SlopeThreshold` (default: 0.7)
- `SlopeBlendWidth` (default: 0.15)
- `AltitudeThreshold` (default: 5000.0)
- `AltitudeBlendHeight` (default: 500.0)

Material instances override these per-biome:
- Island biome: low altitude threshold (sand at sea level)
- Mountain biome: high altitude threshold (snow at peaks)
- Desert biome: disabled slope blend (everything is sandy rock)

## Implementation with VibeUE Tools

### Creating the Slope Blend Function
```python
import unreal

# Create function
func = unreal.MaterialNodeService.create_material_function(
    "MF_Slope_Blend", "/Game/Materials/Functions",
    "Generates a slope mask from world normal", True, ["Landscape"])

# Inputs
unreal.MaterialNodeService.add_function_input(
    func.asset_path, "SlopeThreshold", "Scalar", 0, "Dot product threshold (0.7 ≈ 45°)")
unreal.MaterialNodeService.add_function_input(
    func.asset_path, "BlendWidth", "Scalar", 1, "Transition smoothness")

# Output
unreal.MaterialNodeService.add_function_output(
    func.asset_path, "SlopeMask", 0, "0=flat, 1=steep")

unreal.EditorAssetLibrary.save_asset(func.asset_path)
```

### Creating the Altitude Blend Function
```python
import unreal

func = unreal.MaterialNodeService.create_material_function(
    "MF_Altitude_Blend", "/Game/Materials/Functions",
    "Generates a height mask from world position Z", True, ["Landscape"])

unreal.MaterialNodeService.add_function_input(
    func.asset_path, "HeightThreshold", "Scalar", 0, "World Z height to start blending")
unreal.MaterialNodeService.add_function_input(
    func.asset_path, "BlendHeight", "Scalar", 1, "Height range for transition")

unreal.MaterialNodeService.add_function_output(
    func.asset_path, "HeightMask", 0, "0=below threshold, 1=above")

unreal.EditorAssetLibrary.save_asset(func.asset_path)
```

## Extending the Auto-Layer System

To add a new terrain condition (e.g., moisture/wetness):
1. Create a new mask function `MF_Moisture_Blend`
2. Add moisture mask to the `MF_AutoLayer` combiner
3. Multiply into layer weights alongside slope/height
4. Expose threshold parameters for instance override
