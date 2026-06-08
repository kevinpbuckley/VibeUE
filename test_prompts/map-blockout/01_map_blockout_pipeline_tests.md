# Map Blockout Pipeline Tests

End-to-end tests for `unreal.MapBlockoutService`. Start from an open-world level with a single landscape that has weight layers painted on it (the prompts below assume the landscape is labeled `Landscape1` with paint layers named `Crop`, `Soil`, `Water`, `Forest`). If your landscape uses different names, substitute them in the config below.

Run sequentially. Each `---` block is one prompt.

---

## Discovery

Is there a `MapBlockoutService` available?

---

What methods does `MapBlockoutService` expose? List them.

---

## Stage 0 — Landcover Grid Export

Export the landcover grid for `Landscape1` at the default 120x120 resolution and show me the bounds, the number of layers, and the layer names.

---

The grid you just exported — what's the coverage of the `Crop` layer (sum of weights divided by cell count, as a percentage)? If it's near zero, the rest of the pipeline can't place fields.

---

Save the grid to `<Project>/Saved/MapBlockout/_Test/landcover_grid.json` and confirm the file exists.

---

Now load it back via `LoadLandcoverGridJson` and confirm the round-tripped grid has the same `grid_n`, `world_lo`, `world_hi`, and the same number of layers.

---

## Load the Verkhova Fixture (no live landscape needed)

Load the bundled Verkhova fixture from `Plugins/VibeUE/Source/VibeUE/Tests/MapBlockout/example_data/landcover_grid.json` and report the grid dimensions and layer names.

---

## Stage 1 — Roads

Build a `MapBlockoutConfig` with `level_name="VerkhovaTest"`, seed=7, map the layers `crop=L1, soil=L2, flood=L3, forest=L4` (those are the layer names in the Verkhova fixture). Set `tree_coverage_band = Vector2D(25, 40)` — Verkhova's crop layer is very dense (mean weight 0.627), so the default (30, 40) tree floor leaves no headroom after fields claim 58–59% coverage. The (25, 40) band is the validated working setting for this fixture; the contributor can use (30, 40) on landscapes with less aggressive cropland. Then call `generate_roads` against the Verkhova grid + this config.

How many roads did it produce? How many are Main vs Dirt? Did the Stage 1 gate pass? Show the failing checks if any.

---

What is `stage1.gate.checks[0].name` and `.passed` and `.message`? (Should be `Connectivity`, true, with the largest-component %.)

---

## Stage 2 — POIs

Run `place_pois` with the Stage 1 result. How many POIs did it produce? Show the type distribution (count of each type). Did the gate pass?

---

Confirm there is exactly one `Strongpoint` POI in the result. Report its center and radius.

---

## Stage 3 — Fields

Run `place_fields` with Stage 1 + Stage 2. Report `stage3.coverage_fraction` as a percentage. Did the coverage land in the configured band `field_coverage_band` (defaults to 50–60%)?

---

If Stage 3 fails the `Coverage` check (above or below the band), demonstrate one repair: adjust `field_crop_threshold` up or down and re-run. Report the new coverage and whether the gate passes.

---

## Stage 4 — Foliage

Run `place_foliage` with Stages 1–3. Report `tree_coverage_fraction` as a percentage and the result of the `Forest-Surrounded POI` check (this is critical — the spec requires at least one POI fully ringed by forest).

---

## Stage 5 — Railway and Bridges

Run `place_railway` with Stages 1–4. How long is the railway in km? How many bridges did it find, and what's the breakdown (rail bridges vs road bridges)?

---

## Final Pass + One-Call Pipeline

Run `run_full_pipeline` against the Verkhova grid with the config. Did `final_gate.all_passed` come back true? Report `output_files` — there should be exactly 8 files written to `<Project>/Saved/MapBlockout/VerkhovaTest/`.

---

Confirm the 8 expected files exist on disk:
- `Stage1_Roads.png`
- `Stage2_Roads_POIs.png`
- `Stage3_Roads_POIs_Fields.png`
- `Stage4_Roads_POIs_Fields_Foliage.png`
- `Stage5_Roads_POIs_Fields_Foliage_Rail.png`
- `CombinedFoliageAndMap.png`
- `FoliageHeatMap.png`
- `MapHeatMap.png`

Each PNG should be non-empty (> 1 KB).

---

## Verkhova Smoke Test

The Verkhova fixture is the canonical test input — same data the host-Python `map_designer.py` produces a clean pass against. Running `run_full_pipeline` on it should produce a passing run with:

- ~12 km map (world span ≈ 1,200,000 cm)
- ≥ 15 POIs
- Field coverage in 50–60%
- Tree coverage in 30–40%
- ≥ 1 forest-surrounded POI
- Railway ≥ 0.7 × map km
- 8 deliverable files

Run the pipeline and confirm every one of those holds.

---

## One-Call Convenience: RunFullPipelineForLandscape

If you have a live landscape (not just a JSON fixture), call `run_full_pipeline_for_landscape("Landscape1", config)` and verify it produces the same 8 deliverables. Report any check failures.

---

## River Centerline Extraction (Optional)

If the landscape has a hand-painted water layer, extract a binary water mask from the Stage 0 grid (threshold the flood layer at 0.5), then call `extract_river_centerlines(mask, world_lo, world_hi, min_length_cm=50000)`. How many rivers did it produce, and what's the world-coord range of each river's first/last point?

Feed those rivers into a new config (`cfg.rivers = rivers`) and re-run `run_full_pipeline`. Compare bridge counts against the flood-only run.
