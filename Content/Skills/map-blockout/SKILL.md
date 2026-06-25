---
name: map-blockout
display_name: Procedural FPS Map Blockout
description: Procedurally design an AAA-style open-world FPS map blockout (roads, POIs, fields, forests/treelines, railway/bridges) from a landscape, validate it through gated checks, and materialize it into engine geometry. Use when the user asks to block out or lay out an open-world/FPS map, place roads/POIs/forests/railways procedurally, or turn a blockout plan into splines, paint layers, foliage, and actors.
vibeue_classes:
  - MapBlockoutService
  - LandscapeService
  - FoliageService
unreal_classes:
  - Landscape
  - LandscapeProxy
  - LandscapeSplinesComponent
  - StaticMeshActor
keywords:
  - map blockout
  - level blockout
  - fps map
  - open world
  - arma
  - squad
  - procedural map
  - road network
  - poi
  - point of interest
  - village
  - town
  - farmstead
  - field placement
  - forest placement
  - treeline
  - underbrush
  - scrub
  - railway
  - bridge
  - heatmap
  - color key
  - map designer
  - landcover
  - weight layer
  - paint layer
  - gated design
  - validation gate
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "levels-and-world-partition"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Procedural FPS Map Blockout Skill

Take a VibeUE-generated landscape (heightmap + paint layers) and turn it into a
fully validated, AAA-style open-world FPS map blockout — roads, POIs, fields,
forests/treelines, railway, bridges — then materialize the plan as real engine
geometry. Quality benchmark is **Arma / Squad**.

The design pipeline is **gated**: every stage runs ordered pass/fail checks
against the authoritative spec at [docs/design/map-designer-spec.md](../../../docs/design/map-designer-spec.md).
You may not advance until all checks PASS. On failure, fix the offending element
and re-run the stage.

## Sub-docs available

Skills load via the engine's `AgentSkillToolset` (`ListSkills` / `GetSkills`). To pull a
sub-doc, request `map-blockout/<section>` through `GetSkills`. The sibling `*.md` files also
sit next to this `SKILL.md` on disk and can be read directly.

| Sub-doc | When to load |
|---------|--------------|
| `workflows.md` | End-to-end run: export → stages 1–5 → final pass → materialize. Pick this first. |
| `stage-rules.md` | Per-stage CHECK list (what passes, what fails, how to fix). Mirrors the spec. |
| `config-reference.md` | `FMapBlockoutConfig` fields — coverage bands, road grid, POI targets, tuning knobs. |
| `return-types.md` | Exact USTRUCT property names returned by every service method. |
| `materialization.md` | How the plan becomes splines / paint / actors / foliage in the level. |

## Critical Rules

### The gate is non-negotiable

Each stage's `Gate.bAllPassed` must be `true` before advancing. The orchestrator
(`run_full_pipeline`) stops at the first failing gate and surfaces the failing
check name + message in the returned struct. Inspect `Gate.Checks` to know what
to repair.

### Run Stage 0 first

`run_full_pipeline_for_landscape` is the one-call convenience. Otherwise:

1. `unreal.MapBlockoutService.export_landcover_grid(landscape_label, grid_n=120)`
2. Pass the result + config to `run_full_pipeline`

`export_landcover_grid` reads every paint layer on the source landscape plus the
heightmap, returns a normalized grid in one native call — no PNG round-trip and
no host-Python pre-processing step.

### Map your paint layers to design categories

The spec assumes 4 logical categories: `crop`, `soil`, `flood` (water), `forest`.
You must tell the service which **layer names on the source landscape** play each
role:

```python
cfg = unreal.MapBlockoutConfig()
cfg.layers.crop   = "Crop"     # name on the landscape
cfg.layers.flood  = "Water"
cfg.layers.forest = "Forest"
cfg.layers.soil   = "Soil"
```

Empty `flood` is allowed — water is derived from a low-elevation percentile of
the heightmap.

### Output folder is named after `level_name`

```
<Project>/Saved/MapBlockout/<level_name>/
  Stage1_Roads.png                                   <- cumulative snapshot
  Stage2_Roads_POIs.png
  Stage3_Roads_POIs_Fields.png
  Stage4_Roads_POIs_Fields_Foliage.png
  Stage5_Roads_POIs_Fields_Foliage_Rail.png
  CombinedFoliageAndMap.png                          <- final, with color key
  FoliageHeatMap.png                                 <- trees + fields + scrub
  MapHeatMap.png                                     <- roads + buildings + rail + bridges
```

All 8 files are required for delivery. The Final Pass refuses to ship if any
upstream gate has unresolved failures.

### Color chart is authoritative

Every rendered output uses the MapDesigner color chart (see
[stage-rules.md](stage-rules.md)). Constants live in
`MapBlockoutImage::Colors::*` (C++). Do not invent new colors — the heatmap +
combined map are required deliverables and must match.

| Element | Color |
|---|---|
| Main Roads | White |
| Dirt Roads | Brown |
| Treelines | Light Purple |
| Forest | Dark Purple |
| Buildings | Light Grey |
| Bridges | Orange |
| Railway | Dark Grey |
| POI Boundary | Bright Green Circle |
| Fields | Yellow |
| Underbrush / Scrub | Light Blue |

### The Color Key must NOT cover the map

Top-right panel. If the panel rect intersects the map area, the stage fails its
"Color Key" check. The renderer enforces this — but if you write a custom
renderer, honor the rule.

## Two-Phase Workflow

**Phase 1 — design the plan (CPU, masks + polylines, no engine geometry):**

```python
import unreal
S = unreal.MapBlockoutService

cfg = unreal.MapBlockoutConfig()
cfg.level_name = "Verkhova"
cfg.layers.crop, cfg.layers.forest = "Crop", "Forest"
cfg.layers.flood = "Water"

result = S.run_full_pipeline_for_landscape("Landscape1", cfg)
if not result.success:
    # Inspect result.final_state.<stage>.gate.checks for the failing entry
    print(result.error_message)
else:
    print("Wrote:", result.output_files)
```

**Phase 2 — materialize into the level:**

```python
S.materialize_roads_as_splines(result.final_state.stage1_roads, "Landscape1")
S.materialize_fields_as_paint(result.final_state.stage3_fields, "Landscape1", "Crop")
S.materialize_pois_as_actors(result.final_state.stage2_pois, "/MapBlockout/POIs/")
S.materialize_forest_as_foliage(result.final_state.stage4_foliage,
    "/Game/Foliage/FT_Forest", "/Game/Foliage/FT_Treeline", "/Game/Foliage/FT_Scrub")
S.materialize_railway_and_bridges(result.final_state.stage5_railway, "Landscape1")
```

## Status

Fully implemented — every stage (Roads, POIs, Fields, Foliage, Railway/Bridges),
the Final Pass, the renderers, and the materializers are live in C++. The full
pipeline has been verified end-to-end: all stage gates plus the Final Pass pass,
producing the eight deliverable PNGs.

## Related Skills

| Task | Skills to Load |
|------|---------------|
| Sculpt / paint / create the source landscape first | `landscape` |
| Pull a real-world heightmap before blockout | `terrain-data` |
| Materialize foliage scatter (used by `MaterializeForestAsFoliage`) | `foliage` |
| Place water (rivers, lakes) precisely | `terrain-data` (Mapbox) or `MapBlockoutService.extract_river_centerlines` |
