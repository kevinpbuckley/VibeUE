# Map Maker

A drop-in toolkit that turns a **VibeUE-generated landscape** into a fully validated,
AAA-style FPS map blockout (roads, POIs, fields, forests/treelines, railway + bridges) —
following the gated rules in **[MapDesigner.md](MapDesigner.md)**.

It outputs the heatmaps and per-stage snapshots defined in MapDesigner.md into a folder
**named after your level**. Everything is generated procedurally from your terrain, so it
works on any landscape, not just the example.

> **Prerequisite:** you have already generated/imported a landscape in your level with
> VibeUE — a heightmap plus paint (weight) layers describing land cover (cropland, soil,
> water/floodplain, forest). Map Maker reads those layers; it does not create terrain.

---

## What you get (per level, 8 files)

`Stage1_Roads.png` · `Stage2_Roads_POIs.png` · `Stage3_Roads_POIs_Fields.png` ·
`Stage4_Roads_POIs_Fields_Foliage.png` · `Stage5_Roads_POIs_Fields_Foliage_Rail.png` ·
`CombinedFoliageAndMap.png` · `FoliageHeatMap.png` · `MapHeatMap.png`

A worked example is already in **`Verkhova/`** (generated from `example_data/`, a 12 km
slice of Donetsk, Ukraine).

---

## Folder layout

```
Map Maker/
  README.md                 <- you are here
  MapDesigner.md            <- the gated design spec (rules + checks every stage must pass)
  requirements.txt          <- host Python deps (numpy, scipy, pillow)
  config.example.json       <- copy to config.json and edit
  scripts/
    export_terrain_data.py  <- STAGE 0, runs INSIDE Unreal (VibeUE) -> layer PNGs + manifest
    build_inputs.py         <- STAGE 0, runs on host -> landcover_grid.json
    map_designer.py         <- STAGES 1-5 + Final Pass -> <level>/*.png
  example_data/             <- ready-to-run sample inputs (Donetsk 12 km)
    landcover_grid.json
    river_world.json
  reference/                <- the original validated scripts, for study / advanced use
    verkhova_plan_donetsk_reference.py
    river_centerline_reference.py
  Verkhova/                 <- example output (delete or regenerate)
```

---

## Quick start (try it on the example, no engine needed)

```bash
python -m pip install -r requirements.txt
cd "Map Maker"
python scripts/map_designer.py config.example.json
```

This reads `example_data/` and writes the 8 files into `Verkhova/`, printing a PASS/FAIL
line for every MapDesigner.md check and an `ALL GATES PASSED` summary.

---

## Full workflow on your own terrain

### 1. Stage 0a — export layers from Unreal (VibeUE)
Open `scripts/export_terrain_data.py`, set `LANDSCAPE` (or leave `""` to auto-pick the
first landscape) and `OUT_DIR`, then run it **inside the engine** — either paste it into
`mcp__VibeUE__execute_python_code`, or use *Tools ▸ Execute Python Script* in the editor.
It writes `export_manifest.json`, `export_<Layer>.png` (one per paint layer) and
`export_height.png` to `OUT_DIR`. (Uses only `unreal.LandscapeService` — no host deps.)

### 2. Stage 0b — build the input grid (host)
```bash
python scripts/build_inputs.py --indir <OUT_DIR>
# no painted water layer? synthesize one from elevation:
python scripts/build_inputs.py --indir <OUT_DIR> --flood-from-height
```
This writes `landcover_grid.json` next to the PNGs and prints the available **layer keys**.

### 3. Configure
Copy `config.example.json` to `config.json` and edit:
- `level_name` — output folder name.
- `landcover_grid` / `river_world` — paths to your files (relative to the config).
  `river_world` is **optional**: omit it and water is derived from your `flood` layer.
- `layers` — map `crop` / `soil` / `flood` / `forest` to the layer keys printed in step 2.
- coverage bands + tuning knobs (see below).

### 4. Generate + validate
```bash
python scripts/map_designer.py config.json
```
Read the GATE SUMMARY. If everything passes, your `<level_name>/` folder is the deliverable.

---

## When a gate FAILs — what to turn

`map_designer.py` prints exactly which check failed. Common ones:

| Failing check | Fix in `config.json` (or in-engine) |
|---|---|
| Stage 3 **Field coverage < 50%** | lower `field_crop_threshold`; reduce `road.dirt_spacing_km`; or paint more cropland |
| Stage 3 **Field coverage > 60%** | raise `field_crop_threshold`; add forests (raise `forest_*`) |
| Stage 4 **Tree coverage < 30%** | raise `forest_fringe_iters`; paint more forest layer |
| Stage 4 **Tree coverage > 40%** | lower `forest_fringe_iters` |
| Stage 4 **No Forest-Surrounded POI** | the strongpoint ring is auto-placed; ensure forest layer/headroom near map edges |
| Stage 2 **POI Count too low** | raise `pois.target_count`; reduce `road.dirt_spacing_km` (more intersections) |
| Stage 1 **Connectivity** | orphan road fragments are auto-pruned; if a region is unreachable, reduce `dirt_spacing_km` |

Fields and forests compete for the same cropland: lower the crop threshold to grow fields,
and lean on `forest_fringe_iters` (treeline ringing each wood — it sits in road/forest
margins, so it lifts tree coverage *without* taking field area).

---

## Notes
- **Coordinates:** the world is read from `landcover_grid.json` (`lo`/`hi`, square, origin-centred). 100,000 Unreal units = 1 km.
- **Orientation:** if your generated map looks flipped north–south vs the engine, re-run `build_inputs.py --flip`.
- **Precise rivers + bridges:** the flood-layer water mask is coarse. For crisp river polylines (and rail/road bridge markers placed on real crossings), run `reference/river_centerline_reference.py` on your water layer to produce `river_world.json`, and point `config.json` at it.
- **This is a blockout / design plan**, not in-engine geometry. Use the masks/snapshots as the blueprint to place actual roads (landscape splines), buildings, foliage, and water in Unreal.
