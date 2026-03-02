# Water Features Test — Issues Report

**Test:** Tower Hill Pond, Auburn NH (02_water_features_tests.md)  
**Date:** 2026-03-01  
**Coordinates:** lng=-71.348, lat=42.972, map_size=10 km

---

## Summary

The end-to-end water features test completed the terrain generation and landscape import steps (Steps 1–9) successfully, but the water features workflow (Steps 10–14) had multiple critical failures. No river splines are visible in the viewport, water body planes appear clustered rather than geographically distributed, verification screenshots failed (black images), and the underlying data has significant quality issues.

---

## Issue 1 — Water Body Names/Classes Missing from Raw API Data

**Severity:** High  
**Step:** 10 (Get Water Features)

The `water_features_42.9720_-71.3480_10km.json` file returned by the API contains 16 water bodies, but **all 16 have empty `name` and `class` fields**. Tower Hill Pond is not identifiable by name in the raw data. Only `TowerHillWater.json` (a secondary filtered file) correctly identifies "Tower Hill Pond" — but that file contains only 1 water body and 1 waterway (a single ditch with 2 points), making it useless for the full test.

**Data:**
- 16 water bodies, all with `name=""`, `class=""`  
- The data *does* have ring polygon points (ranging from 42 to 347 points per body)
- The Mapbox vector tile source doesn't include name/class attributes for water polygons in this area

**Impact:** The AI cannot identify which water body is Tower Hill Pond, or distinguish lakes from wetlands, without name/class metadata. The chat log shows the AI treated all 16 unnamed bodies as ponds to place planes for.

---

## Issue 2 — Waterway Names All Empty

**Severity:** Medium  
**Step:** 10

All 244 waterways have **empty `name` fields**. The `class` field IS populated (191 streams, 26 rivers, 12 ditches, 12 intermittent streams, 3 canals), but this wasn't surfaced in the summary returned to the AI.

When the AI printed the water features report, it showed names/classes as blank because the summary format referenced `waterway_class` and `natural_class` field names (from the OSM-style schema) but the actual JSON uses just `class`.

**Data (actual classes present):**
| Class | Count |
|-------|-------|
| stream | 191 |
| river | 26 |
| ditch | 12 |
| stream_intermittent | 12 |
| canal | 3 |

---

## Issue 3 — No River Splines Visible

**Severity:** Critical  
**Step:** 11 (Create River Splines)

The chat log claimed "244 streams/ditch/rivers processed into landscape splines" but **no river splines are visible in the viewport**. Looking at the screenshot, the terrain surface shows no carved channels or spline indicators.

**Possible causes:**
1. **Z-height sampling may have failed** — all `ue5_points` have `Z=0`, and if `LandscapeService.create_spline_from_points()` didn't properly snap control points to the landscape surface, splines may be far below or above the terrain
2. **Coordinate offset miscalculation** — the AI must apply the landscape center offset to the origin-centered UE5 coordinates from `get_water_features`. If the landscape isn't at the origin, the AI needs to add its Location from `get_landscape_info()` to every point
3. **The AI may not have created splines at all** — given the data quality issues, it may have attempted a Python-based approach instead that failed silently
4. **82 of 244 waterways (34%) have only 2 points** — these are single line segments that would create minimal splines, but the remaining 162 waterways should still produce visible results

**Coordinate ranges of waterway UE5 points:**
- X: -877,759 to 565,189
- Y: -716,265 to 702,054

**Expected landscape bounds (ComponentCount 8x8, res 1009, XY scale ~992):**
- ~0 to 999,996 in X and Y (if placed at origin)

The water feature coordinates span -877K to +565K in X and -716K to +702K in Y (centered around 0,0), while the landscape spans 0 to ~1M. This means roughly half the water features fall **outside the landscape bounds** on the negative side.

---

## Issue 4 — Water Planes Clustered in One Corner

**Severity:** Critical  
**Step:** 12 (Create Water Planes)

The user reports water planes are "bunched in one tiny corner" rather than spread across the landscape. The outliner shows `WaterBody_1` through `WaterBody_5` as StaticMeshActors.

**Root cause — coordinate mismatch:** The UE5 coordinates from `get_water_features` are centered around (0,0) = map center. But the landscape placed at the origin has its bounds from ~0 to ~1,000,000 in both axes. The water body positions range from (-900K, -739K) to (588K, 750K), meaning:
- Water bodies in the negative-X or negative-Y quadrant are **entirely outside the landscape**
- Only bodies in the positive quadrant overlap with the landscape
- Even those are shifted ~500K from where they should align on the landscape

Water feature placement is done entirely by the AI via Python `execute_python_code` using `LandscapeService.create_spline_from_points()` for rivers and manual `StaticMeshActor` placement for lake planes. The AI likely **did not apply the landscape offset correction** to the origin-centered coordinates from `get_water_features`, causing all features to be placed at their raw (0,0)-centered coordinates.

**Water body centers (UE5 coordinates, raw):**

| Body | Center X | Center Y | Extent |
|------|----------|----------|--------|
| WB[0] | -688,477 | 596,662 | 389K × 287K |
| WB[1] | -310,787 | 542,248 | 366K × 415K |
| WB[2] | 22,685 | 542,248 | 415K × 415K |
| WB[3] | 378,354 | 489,825 | 409K × 310K |
| WB[4] | -684,108 | 142,274 | 327K × 265K |
| WB[5] | -326,036 | 184,498 | 397K × 415K |
| WB[6] | 22,685 | 184,498 | 415K × 415K |
| WB[7] | 385,651 | 174,279 | 405K × 395K |
| WB[8] | -693,196 | -180,387 | 415K × 401K |
| WB[9] | -338,270 | -107,039 | 386K × 283K |
| WB[10] | 22,685 | -186,240 | 415K × 378K |
| WB[11] | 341,520 | -173,440 | 337K × 415K |
| WB[12] | -708,795 | -531,565 | 384K × 416K |
| WB[13] | -339,494 | -651,451 | 407K × 167K |
| WB[14] | 20,370 | -516,081 | 396K × 385K |
| WB[15] | 380,626 | -531,565 | 415K × 416K |

These extents are unreasonably large (300K–415K UU = 3–4 km each). The 16 water bodies look like **tile-aligned rectangles** — not individual ponds. This is the tile-boundary de-duplication bug (see Issue 6).

---

## Issue 5 — Verification Screenshots Failed (Black Images)

**Severity:** High  
**Step:** 14 (Verify)

Multiple screenshot attempts failed:
1. `TowerHillPond_Verification.png` (38 KB) — **entirely black**. The camera was positioned either inside the terrain or too far away for the Far Clipping Plane to render at that height (Z=800,000).
2. `TowerHillPond_Verification_Final.png` (59 KB) — also failed
3. Multiple `attach_image` tool calls failed with path issues (forward slashes vs backslashes, file not found)

The chat log shows at least **4 failed attach_image attempts** before giving up. The AI tried forward slashes, backslashes, and different file paths, but the image attachment tool kept failing.

**Screenshot file inventory:**
| File | Size | Date | Status |
|------|------|------|--------|
| TowerHill_Verify.png | 51 KB | 2/27 11:07 PM | Earlier test run |
| TowerHill_Verify_V2.png | 39 KB | 2/27 11:07 PM | Earlier test run |
| TowerHill_Final.png | 72 KB | 2/27 11:09 PM | Earlier test run |
| TowerHillPond_Final.png | 256 KB | 2/28 9:14 AM | Likely valid (largest) |
| TowerHillPond_Verification.png | 38 KB | 3/1 7:41 PM | **BLACK** |
| TowerHillPond_Verification_Final.png | 59 KB | 3/1 7:42 PM | **Likely black/corrupt** |

---

## Issue 6 — Water Body De-duplication Failure (Tile Boundaries)

**Severity:** High  
**Step:** 10 (API/Server-side)

The 16 water bodies appear to be **tile-boundary fragments**, not 16 distinct real-world ponds. Evidence:
- All 16 have empty names and classes
- Their extents are 300K–415K UU each (3–4 km) — far larger than any real pond
- They form a grid pattern when plotted by center coordinates
- The `buildFeatureId` function in `water-features.ts` falls back to `${prefix}:${tileX}:${tileY}:${featureIndex}` when no OSM ID is present, meaning **the same water feature spanning multiple tiles gets duplicated** as separate entries

Tower Hill Pond is likely split across multiple tile boundaries and appears as several of these 16 unnamed bodies. Meanwhile, the properly de-duplicated `TowerHillWater.json` correctly identifies it as a single body with 13 polygon points, center at approximately (185K, -347K) UU.

---

## Issue 7 — Spline Application Not Verifiable

**Severity:** Medium  
**Step:** 13 (Apply Splines to Landscape)

The chat log says spline deformations were committed, but since no splines were visible to begin with (Issue 3), it's impossible to verify whether:
- The `ApplySplinesToLandscape` function was called
- The terrain was actually deformed
- The deformation was visible at the current zoom level

---

## Issue 8 — AI Could Not Correctly Read Water Features JSON Fields

**Severity:** Medium  
**Steps:** 11, 12

The AI created intermediate files (`water_features_corrected.json`, `water_features_filtered.json`) that reference `waterway_class`, `natural_class`, and `polygon` field names. The actual JSON fields are `class` and `rings`. This mismatch caused:
- `water_features_corrected.json`: 244 waterways with empty class, 16 water bodies with **0 polygon points**
- `water_features_filtered.json`: 116 waterways with empty class, 16 water bodies with **0 polygon points**

The AI was looking for `polygon` but the data uses `rings` (arrays of ring arrays containing `{lng, lat}` points). The `ue5_rings` field was also available but not used.

---

## Recommendations

### Fix the data pipeline:
1. **Water body de-duplication** — Use geometry hashing or centroid proximity instead of relying solely on OSM IDs. Merge water body polygons that overlap across tile boundaries.
2. **Propagate class data** — Ensure Mapbox waterway `class` values (stream, river, ditch, etc.) are surfaced in the summary returned to the AI, not just in the raw JSON.
3. **Name lookup** — For unnamed water bodies, consider reverse-geocoding or OSM Nominatim lookup to attach names.

### Fix the coordinate alignment:
4. **Standardize the offset** — Either the `get_water_features` function should pre-apply the landscape offset before returning UE5 coords, or the returned JSON should include the required offset so the AI knows to apply it.
5. **Expose landscape center** — The summary returned by `get_water_features` should include the expected landscape center offset, so Python-based water plane placement can use it.

### Fix the screenshot workflow:
6. **Camera positioning** — The screenshot tool should auto-compute a camera position based on landscape bounds (center XY, Z = diagonal/2) rather than relying on the AI to guess coordinates.
7. **attach_image path handling** — Normalize paths to use platform-appropriate separators.

### Fix the spline workflow:
8. **Verify spline creation** — After creating river splines via `LandscapeService.create_spline_from_points()`, use `get_spline_info()` to confirm the world-space positions of created control points are within landscape bounds.
9. **Filter junk waterways** — Skip waterways with only 2 points or class="ditch" to avoid cluttering the landscape with hundreds of tiny segments.
