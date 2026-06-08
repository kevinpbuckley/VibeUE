# Map Blockout — Stage Rules

Authoritative per-stage rules. Mirrors [docs/design/map-designer-spec.md](../../../docs/design/map-designer-spec.md) (the spec is the source of truth — if these two disagree, the spec wins).

Every stage runs its CHECK list in order. **Any single FAIL fails the stage**. The orchestrator stops at the first failing gate and surfaces the failing check name + message via `result.final_state.<stage>.gate.checks`.

---

## Color Chart (authoritative — used in every rendered output)

| Element | Color | C++ constant |
|---|---|---|
| Main Roads | White | `MapBlockoutImage::Colors::MainRoad` |
| Dirt Roads | Brown | `Colors::DirtRoad` |
| Treelines | Light Purple | `Colors::Treeline` |
| Forest | Dark Purple | `Colors::Forest` |
| Buildings | Light Grey | `Colors::Building` |
| Bridges | Orange | `Colors::Bridge` |
| Railway | Dark Grey | `Colors::Railway` |
| POI Boundary | Bright Green | `Colors::POIBoundary` |
| Fields | Yellow | `Colors::Field` |
| Underbrush / Scrub | Light Blue | `Colors::Scrub` |
| River (context only) | Blue | `Colors::River` |

The top-right Color Key panel must NEVER overlap the map area. This is checked on every stage that renders.

---

## Stage 0 — Export Landcover Grid

**OUTPUT:** `FMapBlockoutLandcoverGrid` (square, origin-centred world bounds + N×N normalized weight grid per layer + optional heightmap).

**CHECKS:**
1. **Bounds Present** — `WorldLo`/`WorldHi`/`GridN` populated; span matches the landscape's world size in km.
2. **Layer Mapping** — every category in `Config.Layers` (`crop`, `forest`, and `flood` if water-bridged) resolves to a layer name present in `Grid.Layers`.
3. **Crop Signal** — the crop layer has meaningful coverage (fields come from it). Near-empty crop → either paint more or lower `field_crop_threshold`.
4. **Orientation** — array row 0 = south (`landcover_grid.json` convention); rendered row 0 = north. Conversion happens at the boundary.

**GATE:** all 4 PASS.

---

## Stage 1 — Roadways

**OUTPUT:** `Stage1_Roads.png` + `FMapBlockoutRoadNetworkResult` (Main + Dirt polylines, with color key).

**DO NOT:**
- Produce isolated road fragments that connect to nothing.
- Let the Color Key overlap the map area.

**GUIDELINES:**
- The network should read as a coherent, broadly **grid-like** system that fits the terrain.
- Roundabouts, diagonals, curves permitted where terrain warrants.
- Roads should follow the terrain (hug contours, avoid implausible grades).
- Main Roads = primary skeleton; Dirt Roads = secondary/rural connections.
- Target AAA open-world FPS standards: multiple routes between regions, no dead-end mazes.

**CHECKS (in order):**
1. **Connectivity** — every segment connects into one continuous network. Largest connected component must contain ≥99% of road cells.
2. **Grid Sensibility** — broadly grid-like, fits the terrain.
3. **Pattern Realism** — matches real-world layouts (grid + permitted roundabouts/diagonals/curves).
4. **AAA Standard** — supports FPS movement: multiple routes, readable arteries.
5. **Color Compliance** — Main = White, Dirt = Brown only.
6. **Color Key** — top-right, not covering map.

**GATE:** all 6 PASS. **Do not proceed to Stage 2 until passing.**

---

## Stage 2 — Points of Interest

**OUTPUT:** `Stage2_Roads_POIs.png` + `FMapBlockoutPOIResult` (POI boundary circles + buildings + any new service roads). Builds on Stage 1.

**DO NOT:**
- Place any Building on a Road.
- Place any Building or POI in a River.
- Place POIs off the road network.
- Mismatch POI type with road class (a town reachable only by a dirt track; a farm at a highway interchange).

**GUIDELINES:**
- POIs are the contested FPS objectives — defensible structures, sightlines, cover, multiple approaches, balanced attacker/defender geometry.
- POI type must match its road context (town ↔ main, farm ↔ dirt).
- POIs spread out with natural, non-uniform placement.
- A POI Boundary Circle MAY cross roads (it marks the zone, exempt from the no-overlap rules).

**CHECKS (in order):**
1. **POI Count** — at least **15** for a 12 km map (scale proportionally).
2. **On-Network** — every POI is on the road network.
3. **Type–Road Match** — POI type consistent with its serving road class.
4. **Distribution** — natural spread, no clustering/uniform stamping.
5. **Buildings Off Roads** — no building overlaps any road.
6. **No River Overlap** — no building or POI in a river.
7. **Layout Realism** — building arrangement inside each POI is sensible.
8. **AAA FPS Combat Design** — each POI fightable to AAA standard.
9. **Boundary Circle** — Bright Green, crossing roads allowed (not a failure).
10. **Color Key** — top-right, not covering map.

**GATE:** all 10 PASS.

---

## Stage 3 — Fields

**OUTPUT:** `Stage3_Roads_POIs_Fields.png` + `FMapBlockoutFieldResult` (Yellow fields within the road grid). Builds on Stage 2.

**DO NOT:**
- Overlap any Field with a road, building/POI, or river.
- Let Fields exceed **60%** of the play area.
- Consume so much area that no room remains for treelines and forests.

**GUIDELINES:**
- Fields sit within road-grid cells, agriculturally plausible.
- Leave deliberate headroom for Stage 4 foliage.

**CHECKS (in order):**
1. **Placement Sense** — fields logically inside the road grid.
2. **No Road Overlap.**
3. **No Building/POI Overlap.**
4. **No River Overlap.**
5. **Coverage Bounds** — total field coverage within `field_coverage_band` (default 50–60%).
6. **Foliage Headroom** — enough un-fielded area remains for Stage 4 to satisfy its own coverage rules.
7. **Color Compliance** — Yellow.
8. **Color Key.**

**GATE:** all 8 PASS.

---

## Stage 4 — Trees, Forests, and Underbrush/Scrub

**OUTPUT:** `Stage4_Roads_POIs_Fields_Foliage.png` + `FMapBlockoutFoliageResult` (Treelines = Light Purple, Forest = Dark Purple, Scrub = Light Blue). Builds on Stage 3.

**DO NOT:**
- Overlap trees with Buildings, Roads, Rivers, or Fields — ever.
- Place trees on the road surface (even where a road passes through forest — clear the corridor).
- Let trees + forests exceed **40%** of the map.

**GUIDELINES:**
- Treelines should follow roads (line rural roads, mark boundaries).
- **At least one POI must be fully ringed by forest** (the forest-surrounded strongpoint).
- Roads MAY pass through a forest, but the road surface stays clear.
- Trees may be placed sparsely inside a POI zone (backyard tree, lightly-treed park) **only if** all other rules still hold.
- Forests should read as **irregular, organic woods** — never perfect circles.

**CHECKS (in order):**
1. **No Forbidden Overlap** — no tree/forest overlaps any Building, Road, River, or Field.
2. **Treeline Logic** — treelines follow roads sensibly.
3. **Forest-Surrounded POI** — at least one POI fully ringed by forest.
4. **Road Corridors Clear** — road surfaces stay tree-free even where roads cut through forest.
5. **Coverage Bounds** — trees + forests within `tree_coverage_band` (default 30–40%).
6. **In-POI Trees Valid** — any in-POI trees are sparse and violate no prior rule.
7. **AAA FPS Standard** — foliage supports balanced cover/concealment.
8. **Color Compliance** — Treelines Light Purple, Forest Dark Purple, Scrub Light Blue.
9. **Color Key.**

**GATE:** all 9 PASS.

---

## Stage 5 — Railway and Bridges

**OUTPUT:** `Stage5_Roads_POIs_Fields_Foliage_Rail.png` + `FMapBlockoutRailwayResult` (Railway = Dark Grey, Bridges = Orange). Builds on Stage 4.

**DO NOT:**
- Overlap any Railway or Bridge with a building.
- Place a Bridge anywhere except over water.
- Leave trees standing where the railway intersects a treeline (remove them).

**GUIDELINES:**
- Railways MAY cut through fields.
- Bridges exist solely to carry roads/railway over water.

**CHECKS (in order):**
1. **Railway Placed** — a coherent railway exists, rendered Dark Grey.
2. **No Building Overlap (Rail).**
3. **No Building Overlap (Bridge).**
4. **Bridges Over Water Only** — every bridge is over water; no land bridge.
5. **Treeline Clearance** — every rail/treeline intersection has those trees removed (re-validate Stage 4's intent).
6. **Field Crossing Allowed** — rail crossing fields is fine, not a violation.
7. **Color Compliance** — Railway Dark Grey, Bridges Orange.
8. **Color Key.**

**GATE:** all 8 PASS.

---

## Final Pass — Full Validation & Delivery

**OUTPUT:**
- `CombinedFoliageAndMap.png` (every element, with color key, top-right not covering map)
- `FoliageHeatMap.png` (trees + fields + scrub only; **no color key**)
- `MapHeatMap.png` (roads + buildings + rail + bridges only; **no color key**)
- Plus the 5 per-stage snapshots = **8 files total** in the level folder.

**DO NOT:**
- Generate heatmaps before all stage gates have passed.
- Include a color key on either heatmap.
- Deliver if any single rule from any stage is unmet.

**CHECKS:**
1. **Stage 1 Re-Validation** — all road rules still hold.
2. **Stage 2 Re-Validation** — all POI rules still hold.
3. **Stage 3 Re-Validation** — all field rules still hold.
4. **Stage 4 Re-Validation** — all foliage rules still hold.
5. **Stage 5 Re-Validation** — all rail/bridge rules still hold.
6. **Cross-Layer Integrity** — no new conflict introduced by later stages onto earlier ones.
7. **AAA Fidelity** — output meets AAA open-world FPS standards (Arma / Squad).
8. **Combined Output Valid** — file present, color key top-right, not covering map.
9. **Foliage Heatmap Valid** — present, no color key, only trees/fields/scrub.
10. **Map Heatmap Valid** — present, no color key, only roads/buildings/rail/bridges.
11. **Snapshots Present** — all 5 per-stage snapshots exist.
12. **Delivery** — all 8 files inside the level folder.

**GATE:** all 12 PASS — only then is the level folder a valid deliverable.

---

## Failure → fix matrix (quick reference)

| Failing check | Repair |
|---|---|
| `Connectivity` (Stage 1) | Reduce `road.dirt_spacing_km` |
| `POI Count` (Stage 2) | Raise `pois.target_count` or reduce `road.dirt_spacing_km` |
| `Field coverage < 50%` | Lower `field_crop_threshold`; paint more cropland |
| `Field coverage > 60%` | Raise `field_crop_threshold`; raise `forest_fringe_iters` |
| `Tree coverage < 30%` | Raise `forest_fringe_iters`; paint more forest layer |
| `Tree coverage > 40%` | Lower `forest_fringe_iters` |
| `No Forest-Surrounded POI` | Verify forest layer has signal near map edges |
| `Bridges Over Water Only` | Confirm `cfg.layers.flood` resolves to a real water layer (or use `extract_river_centerlines`) |

Fields and forests **compete for the same cropland**. Lower the crop threshold to grow fields; lean on `forest_fringe_iters` (treeline ring) to lift tree coverage without claiming field area.
