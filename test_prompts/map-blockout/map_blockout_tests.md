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


---

# Map Blockout Materialization Tests

These exercise the `materialize_*` methods that turn a passing pipeline run into actual engine geometry. They require a **live landscape** (the plan-only Verkhova fixture is not enough — materializers need a real `ALandscape` actor to draw splines, paint, and spawn into).

**Prereqs:**
- A live landscape labeled `Landscape1` in the open level
- Weight layers `Crop`, `Forest`, `Water` (or similar) painted on it
- Run the pipeline test sections above end-to-end first; you should already have a passing `result = MapBlockoutService.run_full_pipeline_for_landscape("Landscape1", cfg)` available

Run sequentially. Each `---` is one prompt.

---

## Roads as Splines

Call `materialize_roads_as_splines(result.final_state.stage1_roads, "Landscape1")`. Report `created_count` (number of spline control points placed). It should be in the hundreds for a 12 km map.

---

After that, query the landscape's spline component (`LandscapeService.get_spline_info("Landscape1")`) and confirm the control point and segment counts are non-zero and consistent with the road list.

---

## Fields as Paint

Call `materialize_fields_as_paint(result.final_state.stage3_fields, "Landscape1", "Crop")`. Report `created_count` (number of brush taps applied — should be in the hundreds).

---

Re-export the `Crop` weight layer via `LandscapeService.export_weight_map("Landscape1", "Crop", "<Project>/Saved/MapBlockout/_Test/painted_crop.png")` and confirm the file has non-zero average brightness (i.e. the paint actually landed).

---

## POIs as Actors

Call `materialize_pois_as_actors(result.final_state.stage2_pois, "/MapBlockout/POIs/", building_mesh_path="")`. With an empty mesh path, all building children should be empty placeholders.

Report `created_count` (parent + child actors).

---

In the World Outliner, look under the `MapBlockout/POIs` folder. You should see one `POI_<name>` entry per POI, each with child `<name>_Building_N` placeholders. Confirm a `POI_Strongpoint` parent is present.

---

Now delete those actors and re-run the materializer with a real building mesh (use any small cube/cylinder static mesh you have). Confirm the child actors get the mesh assigned and are scaled by the building's footprint half-extents.

---

## Foliage Scatter

Foliage placement needs three `UFoliageType` assets — pre-create them with the `foliage` skill, then call:

```
materialize_forest_as_foliage(
    result.final_state.stage4_foliage,
    forest_foliage_type_path   = "/Game/Foliage/FT_Forest",
    treeline_foliage_type_path = "/Game/Foliage/FT_Treeline",
    scrub_foliage_type_path    = "/Game/Foliage/FT_Scrub"
)
```

Report `created_count` (total instances across the three tiers).

---

Call `FoliageService.get_instance_count("/Game/Foliage/FT_Forest")` to confirm the forest tier is the densest, then `FT_Treeline`, then `FT_Scrub`.

---

## Railway + Bridges

Call `materialize_railway_and_bridges(result.final_state.stage5_railway, "Landscape1", bridge_mesh_path="")`. Report `created_count` (rail spline points + bridge placeholder actors).

---

Look in the World Outliner under `MapBlockout/Bridges`. Each bridge should be labeled `Bridge_Rail_N` or `Bridge_Main_N` / `Bridge_Dirt_N` based on what it carries.

---

## Cleanup + Re-run Idempotency

The materializers are **not idempotent**. Before re-running:
- Delete the splines: there is no `clear_splines` action yet — delete the spline component manually or recreate the landscape if you need a clean slate
- Delete the POI actors: select the `/MapBlockout/POIs/` folder in the Outliner and delete
- Remove foliage: `FoliageService.remove_all_foliage_of_type(...)` for each of the three foliage types
- Fields paint: `LandscapeService.paint_layer_at_location(...)` with strength = 0 is messy — easier to set the layer fill to 0 explicitly via `LandscapeService.fill_layer` if available, otherwise re-paint blanks

Re-run `materialize_roads_as_splines` and confirm `created_count` matches the first run (idempotent in count, not in side effects).

---

## End-to-End From Scratch

Starting from a fresh open-world level:
1. Create a landscape via `LandscapeService.create_landscape(...)`
2. Add 4 paint layers: `Crop`, `Soil`, `Water`, `Forest`
3. Paint each with reasonable coverage (use `LandscapeService.paint_layer_at_location` or `paint_layer_in_world_rect`)
4. Run `MapBlockoutService.run_full_pipeline_for_landscape("MyLandscape", cfg)`
5. If all gates pass, materialize all five layers (roads, fields, POIs, foliage, rail/bridges)
6. Open the level in the editor and visually confirm the blockout reads as a coherent FPS map

Report any gate failure with the failing check name and message, and any materializer failure with its error message.


---

# Map Blockout Failure Recovery Tests

These deliberately drive the pipeline into failing gates and exercise the repair knobs. The MapDesigner spec's whole point is that every failure is *named* and there's a specific knob to turn — these tests prove that loop works.

Run sequentially. Each `---` is one prompt. Uses the Verkhova fixture so no live landscape is needed.

---

## Setup

Load the Verkhova fixture from `Plugins/VibeUE/Source/VibeUE/Tests/MapBlockout/example_data/landcover_grid.json`. Build a base config (`level_name="VerkhovaRecovery"`, layers `crop=L1, soil=L2, flood=L3, forest=L4`, seed=7) and confirm `run_full_pipeline` passes all gates as a baseline.

---

## Stage 1 Failure — Disconnected Road Network

Increase `road.dirt_spacing_km` to a huge value (e.g. 8.0) so the dirt grid collapses to almost nothing and connectivity fails. Run `generate_roads` only and confirm the `Connectivity` check fails with a largest-component % below 98.5%.

---

Now reduce `road.dirt_spacing_km` back to a sane value (1.7) and re-run. Connectivity should pass. Did `Grid Sensibility` and the other checks all come back true?

---

## Stage 2 Failure — Too Few POIs

Drop `pois.target_count` to 3 (way under the POI_MIN floor for a 12 km map). Run `place_pois` and confirm `POI Count` fails with a count < 15.

---

Restore `pois.target_count` to 16 and confirm `POI Count` passes.

---

## Stage 3 Failure — Field Coverage Too Low

Set `field_crop_threshold` to 0.6 (very strict — almost no cells qualify). Run through Stage 1 → Stage 2 → Stage 3 and confirm the `Coverage` check fails with `coverage < 50%`.

---

Now sweep `field_crop_threshold` down: try 0.4, 0.2, 0.12, 0.08. At what threshold does coverage first land in the 50–60% band? Report.

---

## Stage 3 Failure — Field Coverage Too High

Now go the other way: set `field_crop_threshold` to 0.02 (almost every cell with crop weight qualifies). Re-run Stage 3 and confirm `Coverage` fails on the upper bound (> 60%).

---

Repair by tightening: which knob turn (lower fields or raise forests) gets it back in band? Try `forest_fringe_iters` increase from 9 → 15 and observe.

---

## Stage 4 Failure — Tree Coverage Too Low

Set `forest_fringe_iters` to 0 (no treeline ring around forests). Run through Stages 1–4 and confirm `Coverage 30-40%` fails with tree % below the floor.

---

Sweep `forest_fringe_iters` up (3, 6, 9, 12) and find where coverage lands in the 30–40% band.

---

## Stage 4 Failure — No Forest-Surrounded POI

This is the trickiest check. It requires Stage 2 to have placed a `Strongpoint` POI AND Stage 4 to have grown a forest ring around it (≥85% of the ring area should be forest).

Set up a config where the forest layer has very low signal near the strongpoint (you can simulate by setting `cfg.layers.forest = ""` — no forest source). Run through Stages 1–4 and confirm `Forest-Surrounded POI` fails with ring % below 85.

---

Restore `cfg.layers.forest = "L4"` (the Verkhova fixture's forest layer) and re-run. The check should pass.

---

## Stage 5 Failure — Bridges Over Water Only

This is hard to trigger naturally — the bridge detection only fires at water/land transitions. But you can force it by manually adding a bogus bridge to the result:

```python
rail_result = stage5  # FMapBlockoutRailwayResult
fake = unreal.MapBlockoutBridge()
fake.world = unreal.Vector2D(0, 0)
fake.length_cm = 10000
fake.yaw_degrees = 0
fake.carries = unreal.MapBlockoutRoadType.MAIN
rail_result.bridges.append(fake)
```

Then call `run_final_pass` on a state with the modified rail and confirm the cross-layer integrity catches a bridge on land (or that Stage 5's gate already filtered it — which is the actual spec intent).

---

## Repair Cheatsheet Verification

Run the baseline config again and confirm every check passes. Then deliberately break one knob at a time, observe which named check fails, and apply the documented repair from the skill's [stage-rules.md](../../Content/Skills/map-blockout/stage-rules.md) "Repair" table. Confirm each repair returns the pipeline to passing.

The matrix to verify:

| Knob change | Expected failing check | Repair |
|---|---|---|
| `road.dirt_spacing_km = 8.0` | Stage 1 `Connectivity` | Lower `dirt_spacing_km` |
| `pois.target_count = 3` | Stage 2 `POI Count` | Raise `target_count` |
| `field_crop_threshold = 0.6` | Stage 3 `Coverage` (under) | Lower threshold |
| `field_crop_threshold = 0.02` | Stage 3 `Coverage` (over) | Raise threshold |
| `forest_fringe_iters = 0` | Stage 4 `Coverage` (under) | Raise fringe iters |
| `cfg.layers.forest = ""` | Stage 4 `Forest-Surrounded POI` | Provide a forest layer |

Walk through all six rows and report any that don't behave as documented.

