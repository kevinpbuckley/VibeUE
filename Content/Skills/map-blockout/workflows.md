# Map Blockout — Workflows

End-to-end recipes for taking a landscape from "heightmap painted" to "fully blocked-out playable map."

## Prerequisites

You must already have:

- A landscape actor in the level (created via `LandscapeService.create_landscape` or imported from a real-world heightmap with `terrain_data`).
- **Weight layers painted** on the landscape: at minimum a crop-like layer (drives fields) and a forest-like layer (drives woods). A water layer is optional — water can be synthesized from the low end of the heightmap if not painted.
- The names of those layers (they're whatever you called them — `Crop` / `Grass` / `Farmland` etc.).

If the source landscape has no paint, run the `landscape` skill first to add weight layers.

---

## Workflow A — One-call full pipeline (recommended)

```python
import unreal
S = unreal.MapBlockoutService

cfg = unreal.MapBlockoutConfig()
cfg.level_name = "Verkhova"
cfg.seed = 7

# Map design categories -> actual layer names on the landscape
cfg.layers.crop   = "Crop"
cfg.layers.soil   = "Soil"
cfg.layers.flood  = "Water"
cfg.layers.forest = "Forest"

# Optional tuning (sensible defaults already applied)
cfg.field_coverage_band = unreal.Vector2D(50, 60)
cfg.tree_coverage_band  = unreal.Vector2D(30, 40)
cfg.field_crop_threshold = 0.12
cfg.forest_fringe_iters  = 9
cfg.road.main_verticals  = 3
cfg.road.main_horizontals = 2
cfg.road.dirt_spacing_km = 1.7
cfg.road.diagonals = 3
cfg.pois.target_count = 16
cfg.pois.min_spacing_frac = 0.085

result = S.run_full_pipeline_for_landscape("Landscape1", cfg)

if not result.success:
    # Find the failing stage and check
    state = result.final_state
    for stage_field in ("stage1_roads", "stage2_pois", "stage3_fields",
                        "stage4_foliage", "stage5_railway"):
        stage = getattr(state, stage_field)
        gate = stage.gate
        if not gate.all_passed:
            print(f"FAILED: {stage_field} stage {gate.stage}")
            for chk in gate.checks:
                if not chk.passed:
                    print(f"  - {chk.name}: {chk.message}")
            break
else:
    print(f"Wrote {len(result.output_files)} files to {result.output_dir}")
    for f in result.output_files:
        print("  -", f)
```

On success, `result.output_dir` contains all 8 deliverables. On failure, the
returned struct tells you exactly which check on which stage tripped.

---

## Workflow B — Stage-by-stage with manual repair

Use this when you want to inspect intermediate state, paint corrections between
stages, or build a custom UI around the gate results.

```python
import unreal
S = unreal.MapBlockoutService

# Stage 0
grid = S.export_landcover_grid("Landscape1", grid_n=120)
assert grid.success, grid.error_message

cfg = unreal.MapBlockoutConfig()
cfg.level_name = "Verkhova"
cfg.layers.crop, cfg.layers.forest, cfg.layers.flood = "Crop", "Forest", "Water"

# Stage 1
roads = S.generate_roads(grid, cfg)
if not roads.gate.all_passed:
    # e.g. Connectivity FAIL → reduce dirt_spacing_km and retry
    cfg.road.dirt_spacing_km -= 0.2
    roads = S.generate_roads(grid, cfg)

# Stage 2
pois = S.place_pois(grid, roads, cfg)
# ...etc.

# Stage 3
fields = S.place_fields(grid, roads, pois, cfg)

# Stage 4
foliage = S.place_foliage(grid, roads, pois, fields, cfg)

# Stage 5
rail = S.place_railway(grid, roads, pois, fields, foliage, cfg)

# Build accumulated state for the final pass
state = unreal.MapBlockoutState()
state.config = cfg
state.grid = grid
state.stage1_roads = roads
state.stage2_pois  = pois
state.stage3_fields  = fields
state.stage4_foliage = foliage
state.stage5_railway = rail

final = S.run_final_pass(state)
assert final.all_passed, "Final pass failed — see final.checks"

# Render
out = unreal.Paths.combine(unreal.Paths.project_saved_dir(),
                           "MapBlockout", cfg.level_name)
for stage in range(1, 6):
    S.render_stage_snapshot(stage, state, out)
S.render_final_deliverables(state, out)
```

---

## Workflow C — Materialize the plan into the level

Once a pipeline run has passed every gate, turn the plan into real engine
geometry. Each call is independent — invoke just the ones you want.

```python
S = unreal.MapBlockoutService
state = result.final_state    # from Workflow A or B

# Roads as landscape splines (Main + Dirt placed on separate splines)
S.materialize_roads_as_splines(state.stage1_roads, "Landscape1")

# Fields as paint on the landscape (creates LayerInfo asset if missing)
S.materialize_fields_as_paint(state.stage3_fields, "Landscape1", "Crop")

# POI placeholders — empty actors at each POI center, building placeholders inside
S.materialize_pois_as_actors(state.stage2_pois, "/MapBlockout/POIs/",
                             building_mesh_path="/Game/Buildings/SM_BlockoutCube")

# Forest / treeline / scrub as foliage instances
S.materialize_forest_as_foliage(state.stage4_foliage,
    forest_foliage_type_path   = "/Game/Foliage/FT_Forest",
    treeline_foliage_type_path = "/Game/Foliage/FT_Treeline",
    scrub_foliage_type_path    = "/Game/Foliage/FT_Scrub")

# Railway splines + bridge actors over water
S.materialize_railway_and_bridges(state.stage5_railway, "Landscape1",
                                  bridge_mesh_path="/Game/Bridges/SM_BlockoutBridge")
```

The materializers are **idempotent in intent but not in implementation** — running
them twice will create duplicate splines / actors. Clean the prior run first.

---

## Workflow D — Hand-painted water → river splines

When the source landscape has hand-painted water (not Mapbox-derived), extract
crisp river centerlines for bridge placement:

```python
# 1. Build a binary water mask from the flood layer (Stage 0 has it as a float
#    map; threshold at 0.5 for a simple binary).
grid = unreal.MapBlockoutService.export_landcover_grid("Landscape1", 120)
water_layer = next(L for L in grid.layers if L.layer_name == "Water")

mask = unreal.MapBlockoutMask()
mask.width  = grid.grid_n
mask.height = grid.grid_n
mask.cells  = [1 if w >= 0.5 else 0 for w in water_layer.weights]

# 2. Skeletonize + simplify into polylines
rivers = unreal.MapBlockoutService.extract_river_centerlines(
    mask, grid.world_lo, grid.world_hi, min_length_cm=50000.0)

# 3. Feed them into the config so Stage 5 places bridges on real crossings
cfg.rivers = rivers
```

---

## Failure recovery cheatsheet

| Failing check (returned in `Gate.Checks`) | Knob to turn in `FMapBlockoutConfig` |
|---|---|
| Stage 1 `Connectivity` | Decrease `road.dirt_spacing_km` (more intersections) |
| Stage 2 `POI Count` | Raise `pois.target_count`; or shrink `road.dirt_spacing_km` to add intersections |
| Stage 3 `Field coverage < 50%` | Lower `field_crop_threshold`; or paint more cropland |
| Stage 3 `Field coverage > 60%` | Raise `field_crop_threshold`; raise `forest_*` settings to claim more area |
| Stage 3 `Foliage Headroom` | Field + roads + water + buildings is using too much of the map. Either widen `tree_coverage_band` (e.g. `(25,40)` for crop-heavy fixtures like Verkhova), lower the field band ceiling, or paint the crop layer less aggressively. The Foliage Headroom floor equals `tree_coverage_band[0]`. |
| Stage 4 `Tree coverage < 30%` | Raise `forest_fringe_iters` |
| Stage 4 `Tree coverage > 40%` | Lower `forest_fringe_iters` |
| Stage 4 `No Forest-Surrounded POI` | Ensure forest layer has signal near map edges |
| Stage 5 `Bridges Over Water Only` | Confirm `cfg.layers.flood` resolves to a real water layer |

The same table lives next to the spec at [stage-rules.md](stage-rules.md).
