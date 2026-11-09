---
name: map-blockout/return-types
description: Return-type/data-structure reference for MapBlockoutService results (plan, features, validation reports).
---

# Map Blockout — Return Types

Exact USTRUCT property names returned by each `UMapBlockoutService` method.
Python names are `snake_case`; the C++ names are `PascalCase`. The Python names
are listed here.

## `FMapBlockoutGateResult` (every stage's gate)

| Property | Type | Notes |
|---|---|---|
| `stage` | int | 0..5, or 6 = Final Pass |
| `checks` | array of `FMapBlockoutCheckResult` | One entry per CHECK in the spec, in order |
| `all_passed` | bool | True iff every check passed |
| `failed_count` | int | Count of failed checks |

## `FMapBlockoutCheckResult`

| Property | Type | Notes |
|---|---|---|
| `name` | str | Matches a CHECK name in [stage-rules.md](stage-rules.md) |
| `passed` | bool | |
| `message` | str | What specifically failed (or passed, with context) |

## `FMapBlockoutLandcoverGrid` — Stage 0 output

| Property | Type | Notes |
|---|---|---|
| `success` | bool | False → see `error_message` |
| `landscape_label` | str | Source landscape actor label |
| `grid_n` | int | Grid resolution (square) |
| `world_lo` | float | World min (Unreal units). Square, origin-centred. |
| `world_hi` | float | World max |
| `height_normalized` | array of float | Heightmap downsampled, length = grid_n² |
| `layers` | array of `FMapBlockoutLayerMap` | One entry per paint layer on the landscape |
| `error_message` | str | |

## `FMapBlockoutLayerMap`

| Property | Type |
|---|---|
| `layer_name` | str |
| `grid_n` | int |
| `weights` | array of float (length = grid_n², row-major, **row 0 = south**) |

## `FMapBlockoutRoadNetworkResult` — Stage 1

| Property | Type | Notes |
|---|---|---|
| `success` | bool | |
| `roads` | array of `FMapBlockoutRoad` | Main + Dirt polylines |
| `gate` | `FMapBlockoutGateResult` | Stage 1 checks (6 entries) |
| `error_message` | str | |

## `FMapBlockoutRoad`

| Property | Type | Notes |
|---|---|---|
| `type` | `EMapBlockoutRoadType` | `MAIN`, `DIRT`, `RAILWAY` |
| `points` | array of `Vector2D` | World XY, Unreal units |
| `width_cm` | float | Half-width for spline materialization |

## `FMapBlockoutPOIResult` — Stage 2

| Property | Type | Notes |
|---|---|---|
| `success` | bool | |
| `pois` | array of `FMapBlockoutPOI` | |
| `added_service_roads` | array of `FMapBlockoutRoad` | Roads this stage added to service POIs |
| `gate` | `FMapBlockoutGateResult` | 10 checks |
| `error_message` | str | |

## `FMapBlockoutPOI`

| Property | Type | Notes |
|---|---|---|
| `name` | str | |
| `type` | `EMapBlockoutPOIType` | `TOWN`/`VILLAGE`/`FARMSTEAD`/`CROSSROADS`/`INDUSTRIAL`/`STRONGPOINT` |
| `center` | `Vector2D` | World XY |
| `radius_cm` | float | POI zone radius |
| `buildings` | array of `FMapBlockoutBuilding` | |

## `FMapBlockoutBuilding`

| Property | Type | Notes |
|---|---|---|
| `world` | `Vector2D` | XY of footprint center |
| `half_extents` | `Vector2D` | Half-extents of footprint |
| `yaw_degrees` | float | |

## `FMapBlockoutMask` (used by Stage 3, 4)

| Property | Type | Notes |
|---|---|---|
| `width` | int | |
| `height` | int | |
| `cells` | array of uint8 | Row-major, length = width × height, 0/1 only |

## `FMapBlockoutFieldResult` — Stage 3

| Property | Type | Notes |
|---|---|---|
| `success` | bool | |
| `field_mask` | `FMapBlockoutMask` | |
| `coverage_fraction` | float | 0..1 |
| `gate` | `FMapBlockoutGateResult` | 8 checks |
| `error_message` | str | |

## `FMapBlockoutFoliageResult` — Stage 4

| Property | Type | Notes |
|---|---|---|
| `success` | bool | |
| `forest_mask` | `FMapBlockoutMask` | Dense forest |
| `treeline_mask` | `FMapBlockoutMask` | Light forest ring around forests / along roads |
| `scrub_mask` | `FMapBlockoutMask` | Underbrush |
| `tree_coverage_fraction` | float | Combined Forest + Treeline as fraction of play area |
| `gate` | `FMapBlockoutGateResult` | 9 checks |
| `error_message` | str | |

## `FMapBlockoutRailwayResult` — Stage 5

| Property | Type | Notes |
|---|---|---|
| `success` | bool | |
| `rail_lines` | array of `FMapBlockoutRoad` | `type == RAILWAY` |
| `bridges` | array of `FMapBlockoutBridge` | |
| `gate` | `FMapBlockoutGateResult` | 8 checks |
| `error_message` | str | |

## `FMapBlockoutBridge`

| Property | Type | Notes |
|---|---|---|
| `world` | `Vector2D` | Bridge midpoint |
| `length_cm` | float | Span length |
| `yaw_degrees` | float | |
| `carries` | `EMapBlockoutRoadType` | `MAIN`/`DIRT`/`RAILWAY` |

## `FMapBlockoutPipelineResult` — full pipeline orchestrator

| Property | Type | Notes |
|---|---|---|
| `success` | bool | True iff every gate passed AND all files written |
| `final_state` | `FMapBlockoutState` | The cumulative state (every prior stage) |
| `final_gate` | `FMapBlockoutGateResult` | Final Pass checks (12 entries) |
| `output_files` | array of str | Absolute paths of every file written (8 on success) |
| `output_dir` | str | Resolved output folder |
| `error_message` | str | |

## `FMapBlockoutState`

| Property | Type |
|---|---|
| `config` | `FMapBlockoutConfig` |
| `grid` | `FMapBlockoutLandcoverGrid` |
| `stage1_roads` | `FMapBlockoutRoadNetworkResult` |
| `stage2_pois` | `FMapBlockoutPOIResult` |
| `stage3_fields` | `FMapBlockoutFieldResult` |
| `stage4_foliage` | `FMapBlockoutFoliageResult` |
| `stage5_railway` | `FMapBlockoutRailwayResult` |

## `FMapBlockoutMaterializeResult` — every `materialize_*` method

| Property | Type | Notes |
|---|---|---|
| `success` | bool | |
| `created_count` | int | Spline points / actors / painted cells / foliage instances created |
| `error_message` | str | |
