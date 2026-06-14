---
name: landscape-materials/satellite
description: Satellite-guided landscape material + painting workflow — fetch a satellite image, analyze it, create satellite-informed layers, and paint by slope/height.
---

## Satellite-Guided Material & Painting Workflow

When creating landscape materials for **real-world terrain** (from `terrain_data`), use this workflow to analyze satellite imagery and paint layers that match the real terrain.

### Step 1 — Get satellite image and attach for vision

```
terrain_data(action="get_map_image", lng=..., lat=..., style="satellite-v9")
```

Then **immediately** attach it so you can see the terrain:

```
attach_image(file_path="<path from get_map_image result>")
```

**`attach_image` is a tool call**, not Python. Call it directly like `terrain_data`.

### Step 2 — Analyze the satellite image

After attaching, you will **see** the satellite image in your next response. Analyze it to identify:
- **Terrain features**: rock outcrops, grassland, forest, water, sand, urban areas
- **Color distribution**: what percentage of the terrain is each type
- **Elevation patterns**: where features transition (e.g., rock at peaks, grass in valleys)

Use your visual analysis to choose:
- Layer names (e.g., Rock, Grass, Sand, Forest, Water)
- Base colors that match the satellite colors
- Height/slope thresholds for procedural painting rules

### Step 3 — Create material with satellite-informed layers

Use the standard material creation workflow (see above), but choose layer names and colors based on what you saw in the satellite image.

### Step 4 — Paint using slope/height analysis

Since Unreal Python does NOT have PIL/Pillow, you **cannot** read satellite pixel colors directly. Instead, use your visual understanding of the satellite image combined with terrain analysis:

```python
import unreal

# Get slope map for the landscape
slope_data = unreal.LandscapeService.get_slope_map("MyTerrain")

# Get height data for regions
height_data = unreal.LandscapeService.get_height_in_region("MyTerrain", ...)

# Use slope + height + your satellite visual analysis to set weights:
# - Steep slopes (>40°) → Rock (you saw exposed rock at steep areas)
# - High altitude + moderate slope → Alpine grass (lighter green in satellite)
# - Low altitude + gentle slope → Valley grass (darker green in satellite)
# - Very flat low areas → Sand/dirt (brown/tan in satellite)
unreal.LandscapeService.set_weights_in_region("MyTerrain", "Rock", ...)
unreal.LandscapeService.set_weights_in_region("MyTerrain", "Grass", ...)
```

### ⚠️ Key Rules for Satellite-Guided Painting

1. **Always attach the satellite image** — without seeing it, you're guessing
2. **PIL/Pillow is NOT available** in Unreal Python — don't try to read pixels programmatically
3. **Use visual analysis + slope/height data** — combine what you see with terrain metrics
4. **`import_weight_map`** can import 8-bit grayscale PNGs as per-layer weight masks if available
5. **Describe what you see** in the satellite image before creating painting rules — this validates your analysis
