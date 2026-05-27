# Map Blockout Materialization Tests

These exercise the `materialize_*` methods that turn a passing pipeline run into actual engine geometry. They require a **live landscape** (the plan-only Verkhova fixture is not enough — materializers need a real `ALandscape` actor to draw splines, paint, and spawn into).

**Prereqs:**
- A live landscape labeled `Landscape1` in the open level
- Weight layers `Crop`, `Forest`, `Water` (or similar) painted on it
- Run `01_map_blockout_pipeline_tests.md` end-to-end first; you should already have a passing `result = MapBlockoutService.run_full_pipeline_for_landscape("Landscape1", cfg)` available

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
