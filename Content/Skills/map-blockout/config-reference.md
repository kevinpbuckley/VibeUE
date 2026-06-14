---
name: map-blockout/config-reference
description: Map blockout configuration reference — tunable parameters for roads, POIs, fields, forests, railways, and generation seeds.
---

# Map Blockout — Config Reference

Every field on `FMapBlockoutConfig` and its nested structs, with what each
knob actually does to the gates.

## `FMapBlockoutConfig`

| Field | Default | Effect |
|---|---|---|
| `level_name` | (required) | Output folder name (`<Project>/Saved/MapBlockout/<level_name>/`) |
| `output_dir` | `""` | Override the output directory. Empty = use default under `Saved/`. |
| `layers` | (required) | Maps design categories (`crop`/`soil`/`flood`/`forest`) → real layer names on the source landscape. |
| `seed` | `7` | RNG seed for road/POI placement. Same seed + same grid = identical output. |
| `field_coverage_band` | `(50, 60)` | Required field coverage % of play area. Stage 3 fails outside this band. |
| `tree_coverage_band` | `(30, 40)` | Required tree coverage %. Stage 4 fails outside. |
| `field_crop_threshold` | `0.12` | Min crop-layer weight for a cell to be field-eligible. Lower = more fields. |
| `forest_fringe_iters` | `9` | Dilation width (in cells) of the treeline ring around each forest core. Raise to lift tree coverage without taking field area. |
| `road` | see below | Road network shape parameters. |
| `pois` | see below | POI placement parameters. |
| `rivers` | `[]` | Optional precise river polylines. Empty = water derived from `layers.flood`. |

## `FMapBlockoutLayerKeyMap`

Maps the spec's 4 design categories to whatever you named your weight layers
when painting the landscape.

| Field | Required? | Meaning |
|---|---|---|
| `crop` | yes | Drives where fields are eligible (Stage 3). |
| `soil` | no | Reserved (currently context only). |
| `flood` | no | Water / wet layer. Empty = synthesize from low-elevation percentile of the heightmap. |
| `forest` | yes | Drives where strategic woods are seeded (Stage 4). |

Layer names are matched **case-insensitively** against the source landscape's
paint layers.

## `FMapBlockoutRoadConfig`

| Field | Default | Effect |
|---|---|---|
| `main_verticals` | `3` | Number of vertical paved arteries. |
| `main_horizontals` | `2` | Number of horizontal paved arteries. |
| `dirt_spacing_km` | `1.7` | Dirt-road grid pitch. Smaller → denser network → more intersections → more POI candidates → larger field count → lower per-field area. |
| `diagonals` | `3` | Extra non-grid diagonal connectors. |

## `FMapBlockoutPOIConfig`

| Field | Default | Effect |
|---|---|---|
| `target_count` | `16` | Target POI count for a 12 km map. Pass `0` to auto-scale by area. The Stage 2 gate requires at least 15 for 12 km (scale proportionally). |
| `min_spacing_frac` | `0.085` | Minimum spacing between POIs as a fraction of map span. Raise to spread out, lower to allow denser clusters. |

## Coverage band tuning

The two coverage gates (`field_coverage_band`, `tree_coverage_band`) compete
for the same land. The defaults `(50,60)` + `(30,40)` leave 10–20% headroom
for roads + POIs + water + scrub. If you tighten one band, loosen the other.

| Goal | Adjustment |
|---|---|
| Open agrarian map (Verkhova-style) | `field_coverage_band=(55,65)`, `tree_coverage_band=(20,30)` |
| Forested / hilly | `field_coverage_band=(30,40)`, `tree_coverage_band=(45,55)` |
| Industrial / urban | Lower both, raise POI count, add diagonals |

## Common knob recipes

**More POIs:** lower `road.dirt_spacing_km` (more intersections) AND/OR raise `pois.target_count`.

**Need a forest-ringed strongpoint and Stage 4 can't find one:** raise `forest_fringe_iters` so the treeline reaches around a POI near the forest layer's strongest signal.

**Fields are taking the map:** raise `field_crop_threshold` from `0.12` → `0.20`. This makes field cells harder to qualify.

**Map looks too uniform:** raise `road.diagonals` from `3` → `5`. The dirt grid stays, but diagonal connectors break the regularity.
