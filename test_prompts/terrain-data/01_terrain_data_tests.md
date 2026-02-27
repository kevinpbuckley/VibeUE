# Terrain Data Tests

Tests for the terrain_data tool — real-world heightmap generation, satellite imagery, and full landscape build workflow. Run from the TerrainTest level.

---

## Setup — Delete All Existing Landscapes

Before running any tests, clean the level completely:

1. Load the `landscape` skill
2. Use `LandscapeService.list_landscapes()` to find every landscape in the level
3. Delete **all** of them with `LandscapeService.delete_landscape(label)` for each one
4. Confirm the level has zero landscapes remaining

This ensures every test below creates its own fresh landscape from scratch.

---

## Discovery

What map styles are available for terrain images?

---

## Preview Elevation — Flat Area

Preview the elevation around Tokyo, Japan (lng=139.6917, lat=35.6895).

---

## Preview Elevation — Mountains

Preview the elevation around Mount Fuji (lng=138.7274, lat=35.3606). What height range does it have and what settings do you suggest?

---

## Preview Elevation — Coastal

Preview the elevation for San Francisco (lng=-122.4194, lat=37.7749). Note the mix of hills and bay.

---

## Generate Heightmap — PNG (default)

Generate a heightmap for Mount Fuji. Use the suggested settings from the preview. Save as PNG.

---

## Generate Heightmap — RAW format

Generate a heightmap for Tokyo using format=raw. Use base_level=0 and height_scale=250 since it's very flat.

---

## Generate Heightmap — ZIP format

Generate a heightmap for San Francisco as a ZIP (includes info.txt). Use the suggested settings from the earlier preview.

---

## Generate Heightmap — Mountain Tilt

Generate a heightmap for the Swiss Alps near Zermatt (lng=7.7480, lat=46.0207). Use preview first, then generate with gravity_center=6 so water flows south.

---

## Get Map Image — Satellite

Get a satellite image of Mount Fuji (lng=138.7274, lat=35.3606).

---

## Get Map Image — All Styles

Get map images of San Francisco in all 5 available styles (satellite-v9, outdoors-v11, streets-v11, light-v10, dark-v10).

---

## Get Map Image — Custom Size

Get a satellite image of Zermatt (lng=7.7480, lat=46.0207) at 640x640 pixels.

---

---

## Full Build — Mount Fuji Landscape

*This section builds a complete landscape from real-world data and paints it using the satellite image as a reference. All previous landscapes were deleted in Setup.*

---

### Step 1 — Elevation Preview

Preview the elevation around Mount Fuji (lng=138.7274, lat=35.3606, map_size=17.28). Report the min/max height and the suggested base_level and height_scale.

---

### Step 2 — Generate Heightmap

Generate the heightmap for Mount Fuji using the suggested settings from step 1. Format PNG.

---

### Step 3 — Get Satellite Reference Image

Get a satellite image of Mount Fuji (lng=138.7274, lat=35.3606, style=satellite-v9). Save it alongside the heightmap.

---

### Step 4 — Analyze the Satellite Image

Attach and analyze the satellite image we just downloaded. Identify the main land cover types visible (e.g. snow/ice cap, bare volcanic rock, forest, grassland, developed areas). List what you see and the approximate regions.

---

### Step 5 — Import Heightmap as Landscape

Create a new landscape called "FujiTerrain" by importing the heightmap PNG from step 2. Use:
- QuadsPerSection=63, SectionsPerComponent=2, ComponentCount=8x8
- Set the Z scale so that the full elevation range fits (use the min/max from step 1 to calculate it)
- Place it at the origin

---

### Step 6 — Create Landscape Material

Create a landscape material called M_FujiTerrain in /Game/Terrain/Materials. Add these layers based on what we saw in the satellite image:
- **Snow** — white, apply to highest elevations
- **Rock** — dark grey/brown, bare volcanic rock on steep slopes
- **Forest** — dark green, mid-elevation forested areas
- **Grass** — lighter green, lower slopes and meadows

Connect the layer blend to BaseColor and add a simple height-based roughness (snow=0.8, rock=0.7, forest=0.6, grass=0.5).

---

### Step 7 — Create Layer Info Objects

Create weight-blended layer info objects for Snow, Rock, Forest, and Grass in /Game/Terrain/Layers.

---

### Step 8 — Assign Material to Landscape

Assign M_FujiTerrain to FujiTerrain with all four layer info objects.

---

### Step 9 — Paint Layers from Satellite Reference

Looking at the satellite image of Fuji we analyzed in step 4, paint the FujiTerrain landscape approximately:

- Paint **Snow** at full strength across the summit area (center of the landscape, high elevation)
- Paint **Rock** on the mid-upper slopes surrounding the snow cap
- Paint **Forest** across the lower-mid slopes (the large forested belt around the mountain)
- Paint **Grass** on the outer lower areas and flat surroundings

This should be an approximate visual match to the satellite image — the goal is the right color zones in the right areas, not pixel-perfect accuracy.

---

### Step 10 — Verify

Take a screenshot of the viewport showing FujiTerrain from above at a good angle. Analyze it — does the painted material roughly match the satellite image layout (snow at top, rock below it, forest ring, grass at the base)?

---

---

## Full Build — Grand Canyon Landscape

*Second full build test with a very different terrain type — flat mesa with deep canyon cuts. Delete FujiTerrain first so we start clean.*

---

### Step 1 — Delete Previous Landscape

List all landscapes and delete each one (FujiTerrain from the previous build).

---

### Step 2 — Preview

Preview elevation at the Grand Canyon South Rim (lng=-112.1401, lat=36.0544).

---

### Step 3 — Generate

Generate the heightmap using suggested settings. The canyon has dramatic elevation drops — make sure base_level accounts for it.

---

### Step 4 — Satellite Image

Get the satellite image of the Grand Canyon at the same coordinates.

---

### Step 5 — Analyze

Attach and analyze the satellite. Identify the main visual zones (canyon walls, river, plateau, vegetation).

---

### Step 6 — Import as Landscape

Import as "CanyonTerrain" with appropriate settings. Place at origin.

---

### Step 7 — Material and Paint

Create a material M_Canyon with layers: RedRock, SandRock, River, PlateauGrass. Paint it based on the satellite analysis — red/orange rock walls, sandy canyon floor, green plateau vegetation.

---

### Step 8 — Screenshot and Verify

Take a screenshot from above, analyze it, and confirm the painted zones roughly match the satellite image.

---
