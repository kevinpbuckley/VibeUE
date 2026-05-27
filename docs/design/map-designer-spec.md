# MapDesigner.md

A gated, sequential system for procedurally designing an open-world First-Person Shooter map to AAA fidelity (benchmark: Arma, Squad). Each stage is a **gate**: you produce the output, run the verification checks, and you **may not advance** until every check passes. On any failure, correct the offending elements and re-run the entire check list for that stage.

---

## Toolkit map (how this spec maps to the scripts in this folder)

| This document | Script | Runs where |
|---|---|---|
| **Stage 0** — export terrain | `scripts/export_terrain_data.py` | inside Unreal (VibeUE) |
| **Stage 0** — build inputs | `scripts/build_inputs.py` | host (Python) |
| **Stages 1–5 + Final Pass** | `scripts/map_designer.py` | host (Python) |

`map_designer.py` implements every CHECK and GATE below and prints `PASS`/`FAIL` per check plus a GATE SUMMARY. The **final outputs are written to a folder named after your level** (`level_name` in `config.json`). See `README.md` for the end-to-end run.

---

## How To Read This Document

- **OUTPUT** — the artifact(s) the stage must produce.
- **CHECKS** — ordered, pass/fail verifications. They run top to bottom. Any single FAIL fails the stage.
- **DO NOT** — hard prohibitions. A violation is an automatic FAIL regardless of other checks.
- **GUIDELINES** — qualitative standards used to judge "makes sense" / "AAA standard" checks.
- **GATE** — the explicit stop condition. Do not proceed past a gate with any unresolved FAIL.

### The Correction Loop (applies to every stage)

```
1. Produce OUTPUT for the stage.
2. Run CHECKS in order.
3. If ALL checks PASS -> record PASS, proceed through the GATE to the next stage.
4. If ANY check FAILS:
   a. Identify every failing element.
   b. Correct only those elements (do not regress passing elements).
   c. Re-run the ENTIRE CHECKS list for this stage from the top.
   d. Repeat until ALL checks PASS.
5. Never skip, defer, or partially satisfy a gate.
```

In the toolkit, the "correction" in step 4b means editing `config.json` (road spacing, `pois.target_count`, `field_crop_threshold`, `forest_fringe_iters`, coverage bands) or painting more of the relevant weight layer in-engine, then re-running `map_designer.py`.

---

## Global Reference

### Color Chart (authoritative — used in all rendered outputs)

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

(Water is shown in blue as context; it is terrain you design *around*, not a placed element.)

### Output Files

Each stage saves its **own cumulative snapshot** — a new file that preserves every prior layer and adds the current stage's layer on top. Snapshots are never overwritten; each stage produces a distinct, inspectable map. Every snapshot carries the top-right Color Key, which must **not** cover any part of the map.

**Per-stage snapshots (cumulative — each builds on the last):**

| Stage | File | Layers Shown |
|---|---|---|
| 1 | `Stage1_Roads.png` | Roads |
| 2 | `Stage2_Roads_POIs.png` | Roads + POIs |
| 3 | `Stage3_Roads_POIs_Fields.png` | Roads + POIs + Fields |
| 4 | `Stage4_Roads_POIs_Fields_Foliage.png` | Roads + POIs + Fields + Trees/Forests/Underbrush |
| 5 | `Stage5_Roads_POIs_Fields_Foliage_Rail.png` | Roads + POIs + Fields + Foliage + Railway/Bridges |

**Final deliverables (generated in the Final Pass):**

| File | Contents | When Generated |
|---|---|---|
| `CombinedFoliageAndMap.png` | The full map with every element rendered per the Color Chart (identical layer set to the Stage 5 snapshot, finalized). Includes a **Color Key in the top-right corner** that must **not** cover any part of the map. | Final Pass |
| `FoliageHeatMap.png` | Heatmap of **trees, fields, and underbrush only**. No color key — heatmap only. | Final Pass |
| `MapHeatMap.png` | Heatmap of **roads, buildings, railways, and bridges only**. No color key — heatmap only. | Final Pass |

All eight files are written into a folder **named after the level** (the toolkit's `level_name`).

### Map Constants

- Reference map size: **12 km** play area (scale all density rules to map size).
- All stages operate on the **same coordinate space** so layers align exactly. The toolkit reads the real world bounds from the exported `landcover_grid.json`, so the same rules apply at any map size.

---

# STAGE 0 — Export Terrain Data from VibeUE

You must already have a landscape generated/imported in your level with VibeUE (heightmap + paint/weight layers). This stage pulls that terrain out of the engine into the two host-readable inputs the designer consumes.

### OUTPUT
- `landcover_grid.json` — an `N x N` grid (default 120) of your landscape's weight layers plus the world bounds (`lo`/`hi`). This drives **where fields, forests, and water can go**.
- *(optional)* `river_world.json` — precise water centerlines with width, for crisp rivers and bridge placement. If omitted, water is derived automatically from your **flood/water weight layer**.

### STEPS
1. **In Unreal (via VibeUE), run `scripts/export_terrain_data.py`.** Set `LANDSCAPE` (or leave blank to auto-pick) and `OUT_DIR`, then execute it through `mcp__VibeUE__execute_python_code` or *Tools ▸ Execute Python Script*. It uses `unreal.LandscapeService.export_weight_map()` for each paint layer and `export_heightmap()`, and writes `export_manifest.json` + `export_<Layer>.png` + `export_height.png`.
2. **On the host, run `scripts/build_inputs.py --indir <OUT_DIR>`.** It downsamples the layer PNGs into `landcover_grid.json`. If you have no painted water layer, add `--flood-from-height` to synthesize one from the heightmap (lowest 8% of terrain).
3. **Map your layers in `config.json`.** Set `"layers"` so `crop` / `soil` / `flood` / `forest` point at your exported layer-name keys (printed by `build_inputs.py`).
4. *(optional, for precise rivers)* Run `reference/river_centerline_reference.py` on your exported water layer to produce `river_world.json`.

### CHECKS (in order)
1. **Bounds Present** — `landcover_grid.json` has `lo`, `hi`, `N` and the span matches your landscape size in km.
2. **Layer Mapping** — every category in `config.json "layers"` (`crop`, `forest`, and `flood` if you want water) resolves to a key present in `landcover_grid.json`.
3. **Crop Signal** — the `crop` layer has meaningful coverage (fields come from it). If near-empty, paint more or lower `field_crop_threshold`.
4. **Orientation** — a quick render is not mirrored N–S vs the engine. If it is, re-run `build_inputs.py --flip`.

### GATE
All four checks PASS. Then proceed to Stage 1 (`map_designer.py` performs Stages 1–5 + Final Pass in one run).

---

# STAGE 1 — Roadways

Create the roadway system first. Everything else is placed relative to it.

### OUTPUT
- New snapshot `Stage1_Roads.png` showing the complete road network (Main Roads = White, Dirt Roads = Brown) over the base terrain, with the top-right Color Key present and not covering the map.

### DO NOT
- **DO NOT** produce isolated road fragments that connect to nothing.
- **DO NOT** let the Color Key overlap the map area.

### GUIDELINES
- Research and apply real-world road patterns. The system should read as a **sensical, broadly grid-like** network.
- Roundabouts, diagonal roads, and curved roads are permitted and encouraged where terrain warrants.
- Roads should follow the terrain (avoid implausible grades, hug contours where natural).
- Main Roads form the primary skeleton; Dirt Roads serve secondary/rural connections.
- Target AAA open-world FPS standards: clear lanes of movement, multiple routes between regions, no dead-end mazes unless intentional.

### CHECKS (in order)
1. **Connectivity** — Every road segment connects into one continuous, traversable network (no orphan fragments).
2. **Grid Sensibility** — The network reads as a coherent, broadly grid-like system that fits the terrain.
3. **Pattern Realism** — Road patterns match researched real-world layouts (grid + permitted roundabouts/diagonals/curves).
4. **AAA Standard** — Network supports FPS movement design: multiple routes, readable arteries, terrain-appropriate.
5. **Color Compliance** — Main Roads rendered White, Dirt Roads rendered Brown, no other colors used for roads.
6. **Color Key** — Key present in top-right and not covering the map.

### GATE
All six checks PASS. If any FAIL, run the Correction Loop and re-check from Check 1. **Do not proceed to Stage 2 until all checks PASS.**

---

# STAGE 2 — Points of Interest (POIs)

Analyze the Stage 1 roadway output. Define each POI zone with its boundary circle, then place buildings inside. Add any additional dirt or main roads needed to service the POIs at this time.

### OUTPUT
- New snapshot `Stage2_Roads_POIs.png` building on `Stage1_Roads.png` and adding: POI Boundary circles (Bright Green), Buildings (Light Grey), and any newly added roads. All Stage 1 layers preserved. Color Key in top-right, not covering the map.

### DO NOT
- **DO NOT** place any Building on a Road.
- **DO NOT** place any Building or POI in a River.
- **DO NOT** place POIs off the roadway system (every POI must be served by the road network).
- **DO NOT** place mismatched POIs (e.g., a town reachable only by a single dirt track, a farm sitting on a main highway interchange).

### GUIDELINES
- POIs are the contested objectives of the FPS — lay them out to AAA FPS standards (defensible structures, sightlines, cover, multiple approaches, balanced attacker/defender geometry).
- POI **type must match its road context**: towns on Main Roads, farms on Dirt Roads, etc.
- POIs should be **spread out** across the map with natural, non-uniform placement.
- Building layouts within a POI must be realistic and internally coherent (a village looks like a village; a farmstead looks like a farmstead).
- The **POI Boundary Circle may cross roads** — it marks the POI zone and is exempt from the building/road rules.

### CHECKS (in order)
1. **POI Count** — At least **15 POIs** for a 12 km map (scale proportionally for other sizes).
2. **On-Network** — Every POI is connected to and placed on the roadway system.
3. **Type–Road Match** — Each POI's type is consistent with the road class serving it (town↔main, farm↔dirt, etc.).
4. **Distribution** — POIs are spread out with natural placement (no clustering/uniform-grid stamping).
5. **Buildings Off Roads** — No building overlaps any road.
6. **No River Overlap** — No building or POI sits in a river.
7. **Layout Realism** — Each POI's building arrangement is sensible and realistic.
8. **AAA FPS Combat Design** — Each POI is laid out as a fightable objective meeting AAA FPS standards.
9. **Boundary Circle** — POI Boundary rendered as a Bright Green circle; crossing roads is allowed and not a failure.
10. **Color Key** — Key present in top-right and not covering the map.

### GATE
All ten checks PASS. If any FAIL, run the Correction Loop and re-check from Check 1. **Do not proceed to Stage 3 until all checks PASS.**

---

# STAGE 3 — Fields

Analyze the Stage 1 (roads) and Stage 2 (POIs) outputs. Place Fields per the rules below.

### OUTPUT
- New snapshot `Stage3_Roads_POIs_Fields.png` building on `Stage2_Roads_POIs.png` and adding Fields (Yellow) placed within the road grid. All Stage 2 layers preserved. Color Key in top-right, not covering the map.

### DO NOT
- **DO NOT** overlap any Field with a road.
- **DO NOT** overlap any Field with a building or POI.
- **DO NOT** overlap any Field with a river.
- **DO NOT** let Fields exceed 60% of the play area.
- **DO NOT** consume so much area that no room remains for treelines and forests.

### GUIDELINES
- Field placement should make sense agriculturally and spatially (open fields sit within the defined roadway grid cells).
- Leave deliberate headroom for the foliage stage that follows.

### CHECKS (in order)
1. **Placement Sense** — Fields are logically placed within the roadway grid.
2. **No Road Overlap** — No field overlaps any road.
3. **No Building/POI Overlap** — No field overlaps any building or POI.
4. **No River Overlap** — No field overlaps any river.
5. **Coverage Bounds** — Total field coverage is within **50–60%** of the play area (not below the sensible floor, not above 60%).
6. **Foliage Headroom** — Sufficient un-fielded area remains for treelines and forests (Stage 4 must be able to satisfy its own coverage rules).
7. **Color Compliance** — Fields rendered Yellow.
8. **Color Key** — Key present in top-right and not covering the map.

### GATE
All eight checks PASS. If any FAIL, run the Correction Loop and re-check from Check 1. **Do not proceed to Stage 4 until all checks PASS.**

---

# STAGE 4 — Trees, Forests, and Underbrush/Scrub

Analyze the roadways, POIs, and fields from prior stages. Place Treelines (Light Purple), Forest (Dark Purple), and Underbrush/Scrub (Light Blue) per the rules.

### OUTPUT
- New snapshot `Stage4_Roads_POIs_Fields_Foliage.png` building on `Stage3_Roads_POIs_Fields.png` and adding all foliage layers (Treelines = Light Purple, Forest = Dark Purple, Underbrush/Scrub = Light Blue). All Stage 3 layers preserved. Color Key in top-right, not covering the map.

### DO NOT
- **DO NOT** overlap trees with Buildings, Roads, Rivers, or Fields — ever.
- **DO NOT** place trees on the roadway, even where a road passes through a forest (clear the road corridor of trees).
- **DO NOT** let trees and forests exceed 40% of the map.

### GUIDELINES
- Treelines should **follow roads** and make spatial sense (e.g., line rural roads, mark boundaries).
- **At least one POI** must be surrounded by a forest.
- Roadways **may pass through** a forest, but the road surface itself stays clear of trees.
- Trees **may be placed sparsely inside a POI zone** (e.g., a backyard tree, a lightly-treed park) **only if** all other tree rules are still satisfied.
- Foliage placement must meet AAA FPS standards: cover and concealment that supports balanced engagements, natural-looking distribution. (Forests should read as **irregular, organic woods** — not perfect circles.)

### CHECKS (in order)
1. **No Forbidden Overlap** — No tree/forest overlaps any Building, Road, River, or Field.
2. **Treeline Logic** — Treelines follow roads and read sensibly.
3. **Forest-Surrounded POI** — At least one POI is fully surrounded by forest.
4. **Road Corridors Clear** — Where roads pass through forest, the road surface has no trees on it.
5. **Coverage Bounds** — Trees + forests occupy **no more than 30–40%** of the map.
6. **In-POI Trees Valid** — Any trees inside a POI zone are sparse and violate no prior rule.
7. **AAA FPS Standard** — Foliage supports AAA-standard cover/concealment and natural distribution.
8. **Color Compliance** — Treelines Light Purple, Forest Dark Purple, Underbrush/Scrub Light Blue.
9. **Color Key** — Key present in top-right and not covering the map.

### GATE
All nine checks PASS. If any FAIL, run the Correction Loop and re-check from Check 1. **Do not proceed to Stage 5 until all checks PASS.**

---

# STAGE 5 — Railway and Bridges

Analyze the roadways, POIs, fields, and foliage. Place the Railway system (Dark Grey), then place Bridges (Orange).

### OUTPUT
- New snapshot `Stage5_Roads_POIs_Fields_Foliage_Rail.png` building on `Stage4_Roads_POIs_Fields_Foliage.png` and adding the Railway (Dark Grey) and Bridges (Orange). All Stage 4 layers preserved. Color Key in top-right, not covering the map.

### DO NOT
- **DO NOT** overlap any Railway or Bridge with a building.
- **DO NOT** place a Bridge anywhere except over water.
- **DO NOT** leave trees standing where the railway intersects a treeline (remove those trees).

### GUIDELINES
- Railways **may cut through fields**. Where a railway crosses a treeline, the intersecting trees are removed so the line runs clean.
- Bridges exist solely to carry roads/railway over water.

### CHECKS (in order)
1. **Railway Placed** — A coherent railway system exists and is rendered Dark Grey.
2. **No Building Overlap (Rail)** — Railway overlaps no building.
3. **No Building Overlap (Bridge)** — No bridge overlaps a building.
4. **Bridges Over Water Only** — Every bridge is over water; no bridge exists on dry land.
5. **Treeline Clearance** — Wherever the railway intersects a treeline, those trees have been removed (re-verify Stage 4's no-tree-on-infrastructure intent holds).
6. **Field Crossing Allowed** — Railway crossing fields is present where sensible and is not flagged as a violation.
7. **Color Compliance** — Railway Dark Grey, Bridges Orange.
8. **Color Key** — Key present in top-right and not covering the map.

### GATE
All eight checks PASS. If any FAIL, run the Correction Loop and re-check from Check 1. **Do not proceed to the Final Pass until all checks PASS.**

---

# FINAL PASS — Full Validation & Delivery

Re-validate the entire map against every rule from every stage, confirm AAA fidelity, then generate the heatmaps and ship.

### OUTPUT
- `CombinedFoliageAndMap.png` — the finalized full map (same layer set as the Stage 5 snapshot, validated and cleaned up). Top-right Color Key not covering the map.
- `FoliageHeatMap.png` (trees + fields + underbrush only; heatmap only, no color key) — generated now.
- `MapHeatMap.png` (roads + buildings + railways + bridges only; heatmap only, no color key) — generated now.
- All five per-stage snapshots plus the three files above are placed in a folder named after the level (8 files total).

### DO NOT
- **DO NOT** generate the heatmaps before all stage gates have passed.
- **DO NOT** include a color key on either heatmap.
- **DO NOT** deliver if any single rule from any stage is unmet.
- **DO NOT** let the Color Key cover the map on the combined output.

### GUIDELINES
- Benchmark the result directly against open-world FPS maps from **Arma, Squad, and similar titles**; match their level of fidelity and depth.
- The combined map should read as a believable, contestable, navigable game world — not a diagram.

### CHECKS (in order)
1. **Stage 1 Re-Validation** — All Roadway rules still hold.
2. **Stage 2 Re-Validation** — All POI rules still hold.
3. **Stage 3 Re-Validation** — All Field rules still hold.
4. **Stage 4 Re-Validation** — All Tree/Forest/Underbrush rules still hold.
5. **Stage 5 Re-Validation** — All Railway/Bridge rules still hold.
6. **Cross-Layer Integrity** — No new conflict was introduced by later stages onto earlier ones (full overlap re-scan).
7. **AAA Fidelity** — Output meets AAA open-world FPS standards, comparable to Arma/Squad.
8. **Combined Output Valid** — `CombinedFoliageAndMap.png` correct, Color Key top-right and not covering the map.
9. **Foliage Heatmap Valid** — `FoliageHeatMap.png` shows only trees/fields/underbrush, no color key.
10. **Map Heatmap Valid** — `MapHeatMap.png` shows only roads/buildings/railways/bridges, no color key.
11. **Snapshots Present** — All five per-stage snapshots exist and each correctly shows its cumulative layer set.
12. **Delivery** — All eight files (five snapshots + three final files) are present inside the level folder.

### GATE (final)
All twelve checks PASS. If any FAIL, return to the responsible stage, run that stage's Correction Loop, then re-run this Final Pass from Check 1. **Deliver the level folder only when every check passes.**

---

## Master Stage Order (quick reference)

0. Export terrain from VibeUE → build `landcover_grid.json` (+ optional `river_world.json`) → gate
1. Roadways → gate
2. POIs (boundary + buildings + service roads) → gate
3. Fields → gate
4. Trees / Forests / Underbrush → gate
5. Railway + Bridges → gate
6. Final Pass → validate all → generate heatmaps → deliver `<level>/`

No stage may begin until the previous stage's gate has fully passed.
