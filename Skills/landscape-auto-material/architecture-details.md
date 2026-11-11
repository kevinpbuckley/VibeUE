---
name: architecture-details
description: Deep dive on the auto-layer function chain, mask combination math, slope/altitude function creation, and extension points.
---

## Auto-Layer Architecture Details

### Function Chain

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

### How Masks Combine

Auto-layer uses **multiplicative mask composition**:

```
base_weight  = (1.0 - slope_mask) * (1.0 - height_mask)
slope_weight = slope_mask * (1.0 - height_mask)  // Rock on slopes but not peaks
height_weight = height_mask                        // Snow on peaks regardless of slope
```

This ensures weights always sum to ~1.0 without explicit normalization.

### Height Blend vs. Weight Blend

- **Auto-layer weights** (from slope/altitude) use `LB_HeightBlend` for smooth transitions
- **Painted layers** use `LB_WeightBlend` for artist control
- When both exist on the same landscape, painted layers **override** auto-layer in painted regions

### Creating the Slope Blend Function

Material-function authoring is Epic's: `MaterialNodeService.create_material_function` /
`add_function_input` / `add_function_output` are gone. Create the asset, then add one
`FunctionInput` / `FunctionOutput` expression per pin and set its fields with
`ObjectToolset.SetProperties` (full call shapes in `workflows.md`, Workflow C). All `call_tool`:

```
call_tool MaterialToolset.CreateFunction {"FolderPath": "/Game/Materials/Functions", "AssetName": "MF_Slope_Blend"}
call_tool ObjectToolset.SetProperties    {"Object": {"refPath": "<MF>"},
    "Properties": {"Description": "Generates a slope mask from world normal",
                   "bExposeToLibrary": true, "LibraryCategoriesText": ["Landscape"]}}
```

| Pin | Class | Fields |
|---|---|---|
| in `SlopeThreshold` | `MaterialExpressionFunctionInput` | `InputType: FunctionInput_Scalar`, `SortPriority: 0` — dot-product threshold (0.7 ≈ 45°) |
| in `BlendWidth` | `MaterialExpressionFunctionInput` | `InputType: FunctionInput_Scalar`, `SortPriority: 1` — transition smoothness |
| out `SlopeMask` | `MaterialExpressionFunctionOutput` | `SortPriority: 0` — 0=flat, 1=steep |

Then `call_tool AssetToolset.SaveAsset {"Asset": {"refPath": "<MF>"}}`.

### Creating the Altitude Blend Function

Same shape — `MaterialToolset.CreateFunction` for `MF_Altitude_Blend` ("Generates a height mask from
world position Z", exposed, category `Landscape`), then:

| Pin | Class | Fields |
|---|---|---|
| in `HeightThreshold` | `MaterialExpressionFunctionInput` | `InputType: FunctionInput_Scalar`, `SortPriority: 0` — world Z height to start blending |
| in `BlendHeight` | `MaterialExpressionFunctionInput` | `InputType: FunctionInput_Scalar`, `SortPriority: 1` — height range for transition |
| out `HeightMask` | `MaterialExpressionFunctionOutput` | `SortPriority: 0` — 0=below threshold, 1=above |

Save with `AssetToolset.SaveAsset` before any material calls the function.

### Extending the Auto-Layer System

To add a new terrain condition (e.g., moisture/wetness):
1. Create a new mask function `MF_Moisture_Blend`
2. Add moisture mask to the `MF_AutoLayer` combiner
3. Multiply into layer weights alongside slope/height
4. Expose threshold parameters for instance override
