---
name: terrain-data
display_name: Real-World Terrain
description: Generate heightmaps and map reference images from real-world elevation data using the terrain_data tool. Downloads Mapbox Terrain-RGB tiles server-side and produces Cities:Skylines / UE5-compatible 16-bit grayscale PNG heightmaps.
keywords:
  - terrain
  - heightmap
  - real world
  - real-world
  - elevation
  - topography
  - satellite
  - map image
  - cities skylines
  - mapbox
  - import heightmap
  - landscape import
  - mount fuji
  - san francisco
  - real terrain
  - geographic
  - coordinates
  - latitude
  - longitude
  - topo
---

# Real-World Terrain Skill

Generates heightmaps from real geographic coordinates via the VibeUE terrain API. Requires an active VibeUE API key configured in chat settings.

## Workflow

**Always run in this order:**

1. `preview_elevation` — fetch elevation stats + suggested settings (costs 1 API call)
2. `generate_heightmap` — generate the 16-bit PNG using suggested settings
3. *(optional)* `get_map_image` — satellite/topo reference image for the same area
4. **`attach_image`** — attach the satellite image so you can **see** the terrain colors and features for material/painting decisions
5. Import into UE5 using the `landscape` skill

---

## Test Prompts

```
Create a landscape from the terrain around Mount Fuji
```
```
Generate a heightmap for San Francisco at coordinates -122.4194, 37.7749
```
```
Build a real-world landscape from the Swiss Alps near Zermatt (lng=7.7480, lat=46.0207)
```
```
Get the terrain around the Grand Canyon and make a landscape
```
```
Generate heightmap for Tokyo at 139.6917, 35.6895 with a satellite reference image
```

---

## Step-by-Step Example (Mount Fuji)

### Step 1 — Preview elevation

```
terrain_data(action="preview_elevation", lng=138.7274, lat=35.3606)
```

Response:
```json
{
  "min_height": 340.0,
  "max_height": 3776.0,
  "height_range": 3436.0,
  "suggested_base_level": 340,
  "suggested_height_scale": 27,
  "suggestedZScale": 741,
  "tile_zoom": 13,
  "tile_count": 9
}
```

### Step 2 — Generate heightmap using suggested values

```
terrain_data(
  action="generate_heightmap",
  lng=138.7274, lat=35.3606,
  base_level=340,
  height_scale=27,
  format="png"
)
```

Response:
```json
{
  "success": true,
  "file": "C:/Project/Saved/Terrain/heightmap_35.3606_138.7274.png",
  "min_height_m": 340.0,
  "max_height_m": 3776.0,
  "dimensions": "1081x1081"
}
```

### Step 3 — Get satellite reference image

```
terrain_data(
  action="get_map_image",
  lng=138.7274, lat=35.3606,
  style="satellite-v9"
)
```

Response:
```json
{
  "success": true,
  "file": "E:/Project/Saved/Terrain/map_satellite_v9_35.3606_138.7274.png",
  "style": "satellite-v9",
  "size_bytes": 1981147
}
```

### Step 3b — Attach satellite image for vision analysis

**⚠️ CRITICAL: After downloading a satellite image, ALWAYS attach it so you can see the terrain colors and features.** This lets you make informed decisions about material layers and painting.

```
attach_image(file_path="E:/Project/Saved/Terrain/map_satellite_v9_35.3606_138.7274.png")
```

`attach_image` is a **tool call** (like `terrain_data` or `manage_skills`), NOT a Python function. Do NOT put it inside `execute_python_code`. Call it directly as a tool.

After attaching, you will see the satellite image in your next response. Use it to:
- Identify terrain features (rock, grassland, water, sand, forest, urban)
- Choose appropriate material layer names and colors
- Design accurate procedural painting rules (height/slope thresholds)
- Match real-world color distribution to layer weights

### Step 4 — Import into UE5

Use the `landscape` skill to import the heightmap file:
- Component setup: `QuadsPerSection=63, SectionsPerComponent=1, ComponentCount=8x8` → 505×505 quads → import at 1081×1081 fits the Cities: Skylines format
- Z scale: set to match the `height_scale` used (lower scale = taller mountains need lower Z)
- File: the `.png` path returned in step 2

---

## Parameters Reference

### generate_heightmap

| Parameter | Default | Notes |
|-----------|---------|-------|
| `lng` / `lat` | required | Decimal degrees. Positive = East/North |
| `format` | `png` | `png` = 16-bit grayscale, `raw` = binary, `zip` = PNG + info |
| `map_size` | `17.28` | km — Cities: Skylines standard |
| `base_level` | `0` | Use `suggested_base_level` from preview |
| `height_scale` | `100` | Use `suggested_height_scale` from preview |
| `water_depth` | `40` | Cities: Skylines water units |
| `gravity_center` | `0` | 0=off, 2=N, 4=E, 6=S, 8=W (tilts terrain for water flow) |
| `level_correction` | `0` | 0=none, 2=flatten coastlines, 3=aggressive |
| `blur_passes` | `10` | **Adjust per terrain character!** 5–10=rugged, 15–25=hills, 25–40=smooth/flat. See Terrain Character Guide |
| `plains_height` | `140` | Meters — threshold between plains and mountains |
| `save_path` | auto | Saves to `<ProjectDir>/Saved/Terrain/` by default |

### get_map_image styles

| Style | Description |
|-------|-------------|
| `satellite-v9` | Aerial imagery (best for landscape texturing) |
| `outdoors-v11` | Topo with trails and contours |
| `streets-v11` | Street map |
| `light-v10` / `dark-v10` | Minimal |

---

## UE5 Landscape Import Settings

### Valid Resolutions — Always Pass `resolution=N`

**⚠️ Never use the default 1081×1081 for UE landscapes** — it requires 36×36 components and will timeout. Always pass `resolution=` matching your landscape config.

| resolution= | Components | Quads | Sections | km at UE scale=100 |
|-------------|-----------|-------|----------|--------------------|
| 505         | 8×8       | 63    | 1        | ~0.5 km            |
| 1009        | 8×8       | 63    | 2        | ~1.0 km            |
| 1009        | 16×16     | 63    | 1        | ~1.0 km            |
| 2017        | 16×16     | 63    | 2        | ~2.0 km            |
| 4033        | 32×32     | 63    | 2        | ~4.0 km            |
| 8129        | 32×32     | 127   | 2        | ~8.1 km            |

> "km at scale=100" is the UE world size when landscape XY scale = 100 cm/quad (default). Adjust scale to match real geography (see below).

### Matching Real-World Scale

`terrain_data`'s `map_size` (default: 17.28 km) controls the geographic area captured. To make the UE landscape match actual geography:

```
UE XY Scale (cm/quad) = (map_size_km × 100,000) / (resolution − 1)
```

Example: `map_size=20`, `resolution=2017` → `(20 × 100,000) / 2016 ≈ 992 cm/quad (~9.9 m/quad)`

Set this as the landscape's X and Y scale when creating it.

### Format and Z Scale

- **Format**: 16-bit grayscale PNG
- **Z Scale**: `20000 / height_scale` cm — e.g., `height_scale=27` → Z scale ≈ 741

**⚠️ ALWAYS calculate Z Scale from height_scale. Do NOT guess.** Use this formula when creating the landscape:

```
z_scale = 20000 / height_scale
```

Derivation: pixel encoding is `pixel = elevation_m × heightScale × 64 / 100` and UE interprets `height_cm = pixel × z_scale / 128`. Setting `height_cm = elevation_m × 100` (meters→cm) gives `z_scale = 20000 / heightScale`.

| height_scale | Z Scale | Terrain Type Example |
|-------------|---------|---------------------|
| 27          | 741     | Tall mountains (Mt. Fuji, Alps) |
| 50          | 400     | Moderate mountains (Appalachians) |
| 100         | 200     | Hills, canyons (Grand Canyon) |
| 150         | 133     | Low hills, mesas |
| 200         | 100     | Gentle rolling terrain |
| 250         | 80      | Very flat terrain, gentle domes |

For Cities: Skylines, export as PNG and import via the standard heightmap importer (1081 default is fine there).

---

## Terrain Character Guide

**⚠️ CRITICAL: The `suggested_height_scale` from `preview_elevation` maximizes detail, but also amplifies noise. You MUST adjust `blur_passes` based on the terrain character.**

### Identifying Terrain Character

After `preview_elevation`, look at the `height_range` and think about what the terrain actually looks like:

| height_range | suggested_height_scale | Terrain Character | blur_passes |
|-------------|----------------------|-------------------|-------------|
| > 2000m     | Low (< 50)           | **Rugged mountains** — keep default blur | 5–10 |
| 500–2000m   | Moderate (50–100)    | **Mixed terrain** — moderate smoothing | 10–15 |
| 100–500m    | High (100–200)       | **Hills/mesas** — needs more smoothing | 15–25 |
| < 100m      | Very high (200–250)  | **Flat/gentle** — needs heavy smoothing | 25–40 |

### Smooth vs Rugged Terrain

**Smooth terrain** (granite domes, rolling hills, plains, gentle slopes):
- Use higher `blur_passes` (20–40) to remove data noise
- The high `height_scale` amplifies every pixel of noise — smoothing counteracts this
- Examples: Enchanted Rock, Uluru, sand dunes, prairies

**Rugged terrain** (jagged peaks, canyons, volcanic craters):
- Use lower `blur_passes` (5–10) to preserve detail
- Lower `height_scale` means less noise amplification, so less smoothing needed
- Examples: Grand Canyon, Matterhorn, Iceland lava fields

### Common Mistake: Jagged Smooth Terrain

If you get a terrain that looks jagged/spiky when the real place is smooth:
1. The `height_scale` was too high without enough `blur_passes`
2. Fix: Re-generate with `blur_passes=30` or higher
3. Also check: Z scale should be `20000 / height_scale`, NOT an arbitrary value like 200

---

## Troubleshooting

| Issue | Fix |
|-------|-----|
| `NO_API_KEY` | Set your VibeUE API key in chat settings |
| `429` rate limit | 100 requests/day (free), 1000/day (paid) |
| Flat heightmap | `height_range` < 50m — use `height_scale: 250` for detail |
| Clipped mountains | Lower `height_scale` or increase `base_level` |
| Jagged/spiky terrain | Increase `blur_passes` (20–40 for smooth terrain) and check Z scale formula |
| Terrain doesn't match real place | Check terrain character guide — smooth places need high blur_passes |
| Timeout | Large map sizes at high zoom may hit Vercel's 10s limit — try smaller `map_size` |
