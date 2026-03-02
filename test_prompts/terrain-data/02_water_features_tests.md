# Water Features Test — Tower Hill Pond, Auburn NH

Build a landscape of Tower Hill Pond in Auburn, New Hampshire with realistic water features. The area should be large enough to capture the full pond, surrounding hills, and nearby lakes.

---

## Setup — Clean Level

Delete all existing landscapes from the level so we start clean.

---

## Step 1 — Preview Elevation

Preview the elevation around Tower Hill Pond in Auburn, NH. Report the elevation stats and recommended import settings.

---

## Step 2 — Generate Heightmap

Generate a heightmap for Tower Hill Pond using the recommended settings from step 1. This is gentle New England terrain — rolling hills and a pond — so use appropriate smoothing.

---

## Step 3 — Get Satellite Reference Image

Get a satellite image of the same area for visual reference.

---

## Step 4 — Analyze the Satellite Image

Analyze the satellite image. Identify the main land cover types — water bodies, forests, fields, roads, and anything else visible. This will inform the material layers.

---

## Step 5 — Import Heightmap as Landscape

Import the heightmap as a new landscape called "TowerHillPond". Use the XY scale and Z scale from the elevation preview so the landscape matches real-world geography.

---

## Step 6 — Create Landscape Material

Based on what the satellite image shows, create a landscape material with appropriate layers — water, shoreline, forest, grass, and any other cover types that are visible.

---

## Step 7 — Create Layer Info Objects

Create weight-blended layer info objects for all the material layers.

---

## Step 8 — Assign Material to Landscape

Assign the material to the landscape with all layer info objects.

---

## Step 9 — Paint Layers

Using the satellite image as a guide, paint the landscape layers to match the real-world land cover.

---

## Step 10 — Get Water Features

Fetch water features for the area. Report what water bodies were found.

---

## Step 11 — Verify

Take a screenshot from above. Confirm:
1. The terrain looks like New England rolling hills
2. No giant planes covering the landscape
3. Painted materials roughly match the satellite image

---

## Quick Tests

*Standalone tests that validate water features without a full landscape build.*

### Fetch Water Features

Fetch water features for the Tower Hill Pond area. List the water bodies found.

### Larger Area

Fetch water features for a larger area around Auburn, NH. Are there additional ponds or lakes beyond what the smaller area shows?

### Compare with Satellite

Get a satellite image and water features for the same area. Do the water bodies match what's visible in the satellite image?
