# MapBlockout — Test Fixtures & Reference Implementation

This folder is the **frozen host-Python reference** that `UMapBlockoutService`
ports to C++. It serves three purposes:

1. **Algorithmic spec** — the C++ ports the math in `reference/map_designer.py` cell-for-cell. When the C++ behaviour disagrees with the Python on the same input, the Python wins (it's the validated, gate-passing implementation).
2. **Regression fixture** — `example_data/landcover_grid.json` + `river_world.json` is the contributor's validated Verkhova/Donetsk sample. Running the C++ pipeline against it should produce gate-passing output that visually matches the Python's `Verkhova/` snapshots.
3. **Side-by-side review** — the contributor can run the Python here and compare against the C++ port without leaving the plugin repo.

## Layout

```
Tests/MapBlockout/
  README.md                                <- you are here
  reference/                               <- contributor's Python, unmodified
    README.md                              <- contributor's top-level README
    requirements.txt                       <- numpy, scipy, pillow
    config.example.json                    <- input config template
    export_terrain_data.py                 <- Stage 0a (in-engine; superseded by UMapBlockoutService::ExportLandcoverGrid)
    build_inputs.py                        <- Stage 0b (host; superseded by ExportLandcoverGrid)
    map_designer.py                        <- Stages 1-5 + Final Pass
    river_centerline_reference.py          <- skeletonize water mask -> polylines
    verkhova_plan_donetsk_reference.py     <- original validated reference

  example_data/                            <- ready-to-run sample inputs
    landcover_grid.json                    <- Verkhova (12 km Donetsk slice)
    river_world.json                       <- precise rivers for that area
```

## Running the Python reference

From this folder:

```bash
python -m pip install -r reference/requirements.txt
cd reference
python map_designer.py config.example.json
```

The config defaults read `../example_data/landcover_grid.json` +
`../example_data/river_world.json`, writing the 8 deliverable files into a
`Verkhova/` folder.

## Authoritative spec

[`../../../docs/design/map-designer-spec.md`](../../../../docs/design/map-designer-spec.md) (the contributor's `MapDesigner.md`, preserved verbatim).

Cite this file from any C++ port comment that implements a CHECK, so a future
maintainer can trace back to the rule.

## Mapping Python → C++

| Python (reference) | C++ port |
|---|---|
| `export_terrain_data.py` (in-engine) + `build_inputs.py` (host) | `UMapBlockoutService::ExportLandcoverGrid` |
| `map_designer.py` Stage 1 (roads) | `UMapBlockoutService::GenerateRoads` |
| Stage 2 (POIs) | `PlacePOIs` |
| Stage 3 (fields) | `PlaceFields` |
| Stage 4 (foliage) | `PlaceFoliage` |
| Stage 5 (rail + bridges) | `PlaceRailway` |
| Final Pass | `RunFinalPass` |
| Stage rendering | `RenderStageSnapshot` |
| Heatmaps + combined | `RenderFinalDeliverables` |
| `river_centerline_reference.py` | `ExtractRiverCenterlines` |
| numpy + scipy.ndimage primitives | `MapBlockoutMath::*` |
| PIL.Image + ImageDraw | `MapBlockoutImage::*` |

## Why keep the Python here?

It would be cleaner to delete it once the C++ ports are complete. **Don't.** The
Python is the only validated, gate-passing implementation today. Keep it as the
canonical reference until the C++ has matched it on every Stage 1..5 + Final
Pass check against `example_data/`. Plan to remove it only after the C++ has
shipped a passing run on Verkhova and at least one second test landscape.
