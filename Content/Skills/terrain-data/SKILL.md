---
name: terrain-data
display_name: Real-World Terrain
description: Build terrain from geographic coordinates with aligned heightmaps, orthophotos, feature masks, map and water vectors, and vista backdrops using terrain_data.
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
  - water
  - river
  - lake
  - ocean
  - waterway
  - stream
  - canal
  - water features
  - spline
  - river spline
  - lake spline
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "landscape-and-foliage"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Real-World Terrain Skill

Use `terrain_data` with the VibeUE API key configured in **Editor Preferences → Plugins → VibeUE**. Generated files default to the project's `Saved/Terrain/` directory.

## Aligned workflow at Enchanted Rock

Use longitude **−98.819936**, latitude **30.505025** and a **3 km** square for every step. Heightmaps are now centred exactly on the requested point, and water coordinates use the landscape's own axes (+Y south). Pass `exact_center=false` for what older builds returned: the old off-centre crop on preview and heightmap (up to half a source tile away, about 1 km here), and `get_water_features` coordinates with +Y north. Check the returned `alignment` (`exact` or `legacy`) before overlaying imagery or geometry.

| Step | Tool call | What to use |
| --- | --- | --- |
| 1. Preview | `terrain_data(action="preview_elevation", lng=-98.819936, lat=30.505025, map_size=3)` | Read `minHeight`, `maxHeight`, `suggestedBaseLevel`, `suggestedHeightScale` and `alignment`. |
| 2. Heightmap | `terrain_data(action="generate_heightmap", lng=-98.819936, lat=30.505025, map_size=3, resolution=1009, base_level=0, height_scale=100)` | Import the 16-bit PNG at `file`. For this example the XY scale is 297.619048 cm per quad and Z scale is 200. Use the preview settings if you need a different vertical mapping, and repeat them for the vista. |
| 3. Orthophoto | `terrain_data(action="get_ortho_image", lng=-98.819936, lat=30.505025, map_size=3, size=4096, source="auto", band="both")` | `files.rgb` is one stitched quality-92 JPEG. `files.canopy` is a linear 8-bit PNG vegetation proxy. `manifest` is the saved server manifest (bounds, credit). The result also has `size`, `source` (`naip` or `mapbox`), `source_reason`, `acquisition_years`, `attribution`, `license_note`, `ground_metres_per_pixel` (0.732600733 at 4096) and `import_hints`. Canopy requires NAIP (United States); elsewhere `auto` picks Mapbox and only `rgb` exists. |
| 4. Masks | `terrain_data(action="get_feature_masks", lng=-98.819936, lat=30.505025, map_size=3, resolution=1009, layers="surface,landcover,parking,road_distance", prefer="auto")` | `files` (layer → PNG path), `resolutions` (layer → actual side), `layers` (layer → channel → meaning), `counts`, `notes`, `source` and the saved `manifest`. Surface and landcover are 8-bit RGBA, parking is 8-bit grey, road distance is 16-bit grey in centimetres. Keep all channels linear, preserve RGB under zero alpha, and use lossless compression. |
| 5. Map features | `terrain_data(action="get_map_features", lng=-98.819936, lat=30.505025, map_size=3, categories=["roads","buildings","pois"], min_spacing_m=0, max_features=500, prefer="auto")` | `file` holds full vectors, tags, IDs and provenance. The reply gives `counts`, `truncated`, `notes`, `source`, `attribution`, `license` and `sample_names` (up to three nonempty names per category). In the file read `ue5_point`, `ue5_points` or `ue5_rings` for point, line or polygon geometry (`{x, y, z}` in centimetres, z 0). The first ring is the exterior and subsequent rings are holes. |
| 6. Water | `terrain_data(action="get_water_features", lng=-98.819936, lat=30.505025, map_size=3, detail="high", min_spacing_m=0)` | Read the saved JSON at `file` for complete lines and rings. The reply lists water bodies, waterways and point counts. Omit both options for the original server detail and spacing (at Enchanted Rock: 14 waterways / 223 points by default, 36 / 439 with `detail="high", min_spacing_m=3`). Water comes from whole source tiles, so some of it lies outside the landscape square. |
| 7. Vista | `terrain_data(action="generate_vista_heightmap", lng=-98.819936, lat=30.505025, map_size=3, resolution=1009, base_level=0, height_scale=100, vista_factor=8)` | `file` and `alignment_file` describe a 24 km backdrop. Use `suggested_xy_scale` (2380.952381 cm), `suggested_z_scale` (200), fractional `inner_rect`, and `clipped` / `clipping`. Match the playable landscape's centre, Z location and processing settings. |

Keep the returned **attribution** and **license** (or **license_note**) with the saved files and derived assets (map features, masks and orthophotos return them; heightmaps, vistas and water do not). Display imagery attribution when presenting the imagery. `get_map_image` provides a quick reference picture and `list_styles` lists its available styles.

### Files, sources and waiting

- Every `file` path in a reply is a full path under the project's `Saved/Terrain/`.
- The default names of heightmap (`heightmap_<lat>_<lng>.png`), water (`water_features_<lat>_<lng>_<size>km.json`) and map features (`mapfeatures_<lat>_<lng>_<size>km.json`) depend only on the place: a second call with other options (an exact and a legacy heightmap, default and high-detail water) overwrites the first. Pass `save_path` to keep both. Masks, orthophotos and vistas get a unique id in their names, and masks and orthophotos ignore `save_path`.
- `source` says where vectors and masks came from: `openstreetmap`, or `mapbox-streets-v8` when the OpenStreetMap servers do not answer (then `notes` says so: generalised geometry, roads cut into tile fragments, no building names, no OSM ids). `prefer="openstreetmap"` fails with `HTTP_502` instead of falling back; `prefer="tiles"` asks for the fallback directly.
- Orthophotos take time: a NAIP tile (2048 pixels; a 4096 image has four per band, an 8192 image sixteen) answers in under a second when its render is still warm and in 20-30 s when it is not, so a 4096 image takes about a minute and an 8192 image several. A tile that answers 502 or 504, or does not answer, is retried up to three times. If your client gives up waiting, call again: tiles that finished rendering meanwhile usually come back quickly (measured at Enchanted Rock: 4096 in 45-55 s, 8192 in 3.6 minutes, one tile in each run answered 502 after 30 s and came back on the second try).

### Registration and request options

- Import north-first rows with **+X east and +Y south**, centred at the world origin: north is **negative Y**. Map features and (by default) water features use these axes. Water JSON saved by older builds, and `get_water_features(exact_center=false)`, use +Y north instead: fetch it again, or negate Y, for use on the landscape. Add the landscape centre offset to vector XY for a translated landscape.
- On a 1009-vertex landscape, the summit at (−98.818824, 30.506464) falls at fractional vertex (539.835480, 450.176183), or (+10665.32, −16018.99) cm. Use unrounded XY scale `100000 * map_size / (N - 1)` for alignment.
- At landscape vertex (i,j), sample a mask or orthophoto of actual side M at `((0.5+i*(M-1)/(N-1))/M, (0.5+j*(M-1)/(N-1))/M)`, where N is the landscape vertex count per side. Road distance caps M at 2017.
- Map features and masks accept up to 40 km and `prefer="auto" | "openstreetmap" | "tiles"`. Vector categories can be a comma list or JSON array of roads, tracks_paths, railways, buildings, parking, landcover, barriers, pois and water. Omit categories for the server's extent-dependent default. An explicit empty array selects none.
- Masks default to 1009 samples and all four layers. Surface packs roads, tracks, water and buildings. Landcover packs rock, woodland, scrub/wetland and grass/agriculture. Parking is a single coverage channel.
- Orthophotos accept sizes 1024, 2048 (default), 4096 and 8192, sources auto, naip or mapbox, and bands rgb (default), canopy or both. Retryable tiles get at most three attempts. Auto RGB can restart the entire image from Mapbox if NAIP repeatedly fails.
- Vista accepts the heightmap processing and format options. The outer footprint is `min(100, map_size * vista_factor)` km. If applying the clipping report's suggested vertical mapping, regenerate both heightmaps. For a seamless mesh cutout, join the playable perimeter vertices to the first exterior vista row.

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
  "minHeight": 340.0,
  "maxHeight": 3776.0,
  "heightRange": 3436.0,
  "suggestedBaseLevel": 340,
  "suggestedHeightScale": 27,
  "suggestedZScale": 741,
  "suggestedXYScales": { "505": 3429, "1009": 1714, "2017": 857, "4033": 429, "8129": 213 },
  "tileZoom": 13,
  "tileCount": 9,
  "alignment": "exact"
}
```

Preview XY scales are rounded. Use `100000 * map_size / (resolution - 1)` for exact registration when calling `create_landscape`. Use `suggestedZScale` for the Z scale.

### Step 2 — Generate heightmap using suggested values

```
terrain_data(
  action="generate_heightmap",
  lng=138.7274, lat=35.3606,
  base_level=340,
  height_scale=27,
  resolution=1009, format="png"
)
```

Response:
```json
{
  "success": true,
  "file": "C:/Project/Saved/Terrain/heightmap_35.3606_138.7274.png",
  "min_height_m": 340.0,
  "max_height_m": 3776.0,
  "dimensions": "1009x1009",
  "alignment": "exact"
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

`attach_image` is a **tool call** (like `terrain_data`), NOT a Python function. Do NOT put it inside `execute_python_code`. Call it directly as a tool. If no `attach_image` tool is available in your session, skip this step and proceed using the saved file path returned by `get_map_image`.

After attaching, you will see the satellite image in your next response. Use it to:
- Identify terrain features (rock, grassland, water, sand, forest, urban)
- Choose appropriate material layer names and colors
- Design accurate procedural painting rules (height/slope thresholds)
- Match real-world color distribution to layer weights

### Step 4 — Import into UE5

Use the `landscape` skill to import the heightmap file:
- Component setup: `QuadsPerSection=63, SectionsPerComponent=2, ComponentCount=8x8` → 1009×1009 resolution
- **X and Y scale**: `100000 * map_size / (resolution - 1)` (1714.2857 for a 17.28 km map at 1009; `suggestedXYScales["1009"]` from preview is the same value rounded to 1714, which is fine until vectors have to line up). **Do NOT use the default 100** — that makes the landscape ~17x too small!
- **Z scale**: Use `suggestedZScale` from preview (e.g. 741 for Mount Fuji)
- File: the `.png` path returned in step 2

Example:
```python
result = unreal.LandscapeService.create_landscape(
    location=unreal.Vector(0, 0, 0),
    rotation=unreal.Rotator(0, 0, 0),
    scale=unreal.Vector(1714.2857, 1714.2857, 741),  # XY = 100000 * map_size / (resolution - 1), Z from suggestedZScale
    sections_per_component=2,
    quads_per_section=63,
    component_count_x=8,
    component_count_y=8,
    landscape_label="MyLandscape"
)
```

---

## Parameters Reference

### generate_heightmap

| Parameter | Default | Notes |
|-----------|---------|-------|
| `lng` / `lat` | required | Decimal degrees. Positive = East/North |
| `format` | `png` | 16-bit grayscale PNG, big-endian uint16 RAW (byte-swap for native little-endian import), or ZIP containing PNG and info |
| `map_size` | `17.28` | km — Cities: Skylines standard |
| `resolution` | `1081` | Samples per side, 33–4097. Pass the landscape's vertex count (505, 1009, 2017, ...) |
| `exact_center` | `true` | Centre the square exactly on `lng` / `lat`; the reply says `alignment: "exact"`. `false` = the crop older builds returned (`alignment: "legacy"`), which does not line up with masks, orthophotos or vectors |
| `base_level` | `0` | Use `suggestedBaseLevel` from preview |
| `height_scale` | `100` | Use `suggestedHeightScale` from preview |
| `water_depth` | `40` | Cities: Skylines water units |
| `gravity_center` | `0` | 0=off, 2=N, 4=E, 6=S, 8=W (tilts terrain for water flow) |
| `level_correction` | `0` | 0=none, 2=flatten coastlines, 3=aggressive |
| `blur_passes` | `10` | **Adjust per terrain character!** 5–10=rugged, 15–25=hills, 25–40=smooth/flat. See Terrain Character Guide |
| `plains_height` | `140` | Meters — threshold between plains and mountains |
| `save_path` | auto | Saves to `<ProjectDir>/Saved/Terrain/` by default |

### get_map_image styles

| Style | Description |
|-------|-------------|
| `satellite-v9` | Quick aerial reference |
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

For absolute elevations, set the actor Z at local zero to `100 * base_level + 32768 * z_scale / 128` cm. Use the same local datum offset for the inner map and vista.

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

**⚠️ CRITICAL: The `suggestedHeightScale` from `preview_elevation` maximizes detail, but also amplifies noise. You MUST adjust `blur_passes` based on the terrain character.**

### Identifying Terrain Character

After `preview_elevation`, look at the `heightRange` and think about what the terrain actually looks like:

| heightRange | suggestedHeightScale | Terrain Character | blur_passes |
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

## Water Features Reference

### get_water_features

Fetches waterways and water bodies from Mapbox vector tiles. Use the **exact same `lng`, `lat`, and `map_size`** as your heightmap call.

```
terrain_data(action="get_water_features", lng=-105.0, lat=39.7, map_size=17.28)
```

Response includes:
- `file` — path to the saved JSON file (e.g. `Saved/Terrain/water_features_42.9720_-71.3480_10km.json`)
- `num_waterways` / `num_water_bodies` — counts
- `waterway_class_breakdown` — object with class counts, e.g. `{"stream": 191, "river": 26, "ditch": 12}`
- `waterways[]` — summary of each waterway: `name`, `class`, `estimated_width_m`, `num_points`
- `water_bodies[]` — summary of each water body: `name`, `class`, `num_ring_points`
- `message` — instructions on how to use the saved file, coordinate system info
- `ue5_coordinate_note` — critical info about coordinate offset for water planes

The **saved JSON file** contains the full data with:
- `waterways[]` — rivers, streams, canals. Each has `name`, `class`, `estimated_width_m`, `points` (lng/lat array), **`ue5_points`** (array of `{x, y, z}` objects in UE5 coords)
- `water_bodies[]` — lakes, ponds, oceans. Each has `name`, `class`, **`rings`** (lng/lat array of arrays), **`ue5_rings`** (array of arrays of `{x, y, z}` objects in UE5 coords)

**⚠️ CRITICAL FIELD NAMES:**
- Waterway positions: `ue5_points` (NOT `points` — those are lng/lat)
- Water body polygons: `ue5_rings` (NOT `polygon` or `ring`)
- Waterway type: `class` (NOT `waterway_class` or `type`)

### Coordinate system

`ue5_points` and `ue5_rings` are **landscape-center-relative** (origin-centered):
- Map geographic center = **(0, 0, Z)** in UE5 space
- **+X = East, +Y = South** (north is negative Y, as on a landscape imported from the heightmap), 1 meter = 100 UU, `z` is 0
- With `exact_center=false` the file is the one older builds wrote: **+Y = North** (mirrored north-south on the landscape), and the reply's `ue5_coordinate_note` says which convention the file uses

---

## Troubleshooting

| Issue | Fix |
|-------|-----|
| `429` rate limit | Read the error for the daily request quota or hourly tile/layer allowance and its UTC reset |
| `HTTP_502` / `HTTP_504` | An upstream source (OpenStreetMap, NAIP, Mapbox) failed or was too slow. Call again; for vectors use `prefer="auto"` |
| Flat heightmap | `heightRange` < 50m — use `height_scale: 250` for detail |
| Clipped mountains | Lower `height_scale` or increase `base_level` |
| Jagged/spiky terrain | Increase `blur_passes` (20–40 for smooth terrain) and check Z scale formula |
| Terrain doesn't match real place | Check terrain character guide — smooth places need high blur_passes |
| Timeout | Reduce the extent, resolution or filter passes for the operation |
| No waterways returned | Area may have no mapped water features (desert, urban grid) — check satellite image |
| Landscape too small | You used XY scale=100 (default) instead of the correct value from `suggestedXYScales`. Recreate with the correct scale |
| Black screenshots after camera move | Camera inside terrain. Use `ActorService.get_actor_view_camera()` to frame from TOP view instead of guessing Z height |
| Water features timeout | PBF tile fetch uses 45s timeout — large map_size at high zoom may still be slow |
| Field name errors | Use `ue5_points` (not `points`), `ue5_rings` (not `polygon`), `class` (not `waterway_class`) |

