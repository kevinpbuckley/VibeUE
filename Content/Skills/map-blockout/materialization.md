# Map Blockout — Materialization

The MapBlockout pipeline produces a **plan** (masks, polylines, POI lists). The
materializers turn that plan into actual engine geometry. Each `materialize_*`
method is an independent step — invoke just the ones you want.

All five materializers take pieces of `result.final_state` (from
`run_full_pipeline`) and an output target (a landscape label, a foliage type
path, a world folder).

---

## `materialize_roads_as_splines`

```python
result = S.materialize_roads_as_splines(
    state.stage1_roads,    # FMapBlockoutRoadNetworkResult
    "Landscape1"            # source landscape's actor label
)
```

**What it does:**
1. Validates the stage's `gate.all_passed`.
2. Groups roads by `type` (Main / Dirt — railway has its own materializer).
3. For each polyline, calls `ULandscapeService.create_spline_point()` at every
   point, then `connect_spline_points()` between consecutive points.
4. Applies different spline-mesh entries per road type so paved roads carry the
   paved mesh and dirt roads carry the dirt mesh. (Mesh paths are configured
   on the landscape itself — see [landscape](../landscape/SKILL.md) skill's
   `set_spline_segment_meshes`.)

**What it doesn't do:**
- Sculpt the terrain along the spline. Splines can raise/lower terrain via
  `b_raise_terrain` / `b_lower_terrain` — that's a downstream decision the
  contributor wires in.
- Pick spline mesh assets. The caller is expected to have configured the
  landscape's spline mesh entries beforehand.

---

## `materialize_fields_as_paint`

```python
result = S.materialize_fields_as_paint(
    state.stage3_fields,    # FMapBlockoutFieldResult
    "Landscape1",
    "Crop"                  # paint layer name on the landscape
)
```

**What it does:**
1. Checks the target paint layer exists. If not, creates a `LandscapeLayerInfoObject`
   via `ULandscapeMaterialService.create_layer_info_object()` and adds it with
   `ULandscapeService.add_layer()`.
2. Walks `fields.field_mask`: for every cell == 1, computes the world XY and
   calls `ULandscapeService.paint_layer()`.

**Performance note:** cell-by-cell `paint_layer` is slow for large maps. The
intended optimization is a bulk-paint API (`paint_layer_from_mask`) on
`ULandscapeService` — separate ticket; the contributor can either rely on the
slow path during the skeleton phase or add the bulk method while porting.

---

## `materialize_pois_as_actors`

```python
result = S.materialize_pois_as_actors(
    state.stage2_pois,                   # FMapBlockoutPOIResult
    "/MapBlockout/POIs/",                # World Outliner folder path
    building_mesh_path="/Game/Buildings/SM_BlockoutCube"  # optional
)
```

**What it does:**
1. For each POI, spawns a parent placeholder actor at `poi.center`, labeled
   `POI_<Type>_<N>`, placed under the requested World Outliner folder.
2. For each `Building` inside the POI, spawns a child
   `AStaticMeshActor` at `building.world`, rotated by `yaw_degrees`, with the
   provided mesh. If `building_mesh_path` is empty, child actors are empty
   placeholders.
3. A debug billboard / sphere component representing `poi.radius_cm` is attached
   to the parent so the POI zone is visible in the editor.

**Replacement workflow:** the placeholders are **blockout markers**. The
contributor swaps them for finished prefabs later (e.g. by reading the actor
labels and bulk-replacing meshes via `ActorService`).

---

## `materialize_forest_as_foliage`

```python
result = S.materialize_forest_as_foliage(
    state.stage4_foliage,
    forest_foliage_type_path   = "/Game/Foliage/FT_Forest",
    treeline_foliage_type_path = "/Game/Foliage/FT_Treeline",
    scrub_foliage_type_path    = "/Game/Foliage/FT_Scrub"
)
```

**What it does:**
1. For each mask (`forest_mask`, `treeline_mask`, `scrub_mask`), samples Poisson-disk
   positions inside the `1` cells (denser for forest, sparser for scrub).
2. Calls `UFoliageService.add_instances()` for each foliage type at the sampled
   positions.

**Foliage type assets:** must already exist. If not, create them via the
[foliage](../foliage/SKILL.md) skill first, or pass empty paths to skip a tier.

**Alternative — procedural via `LandscapeGrassType`:** if the landscape's
material already uses procedural grass via `LandscapeGrassType` weight-driven
scatter, `materialize_fields_as_paint` (above) is enough — the grass spawns
itself from the painted Crop layer. In that case skip this method.

---

## `materialize_railway_and_bridges`

```python
result = S.materialize_railway_and_bridges(
    state.stage5_railway,
    "Landscape1",
    bridge_mesh_path="/Game/Bridges/SM_BlockoutBridge"
)
```

**What it does:**
1. Same spline pipeline as `materialize_roads_as_splines` but writes a railway
   tier (`type == RAILWAY`).
2. For each `Bridge`, spawns an `AStaticMeshActor` at `bridge.world`, rotated
   `(0, yaw_degrees, 0)`, X-scaled from `length_cm`. Empty `bridge_mesh_path`
   spawns a placeholder cube.

---

## Order of materialization

Independent calls — order doesn't matter mathematically. But for **visual
debugging** in the editor, prefer this order:

1. **Roads first** (so the network is visible while POIs and fields appear over it).
2. **Fields** (paint).
3. **POIs** (placeholders).
4. **Foliage** (last big batch).
5. **Railway and bridges** (final pass — they cross everything).

## Cleanup before re-running

The materializers are **not idempotent** — running them twice creates duplicate
splines / actors / instances. Before re-running on the same landscape:

- Splines: `ULandscapeService.clear_splines(landscape_label)` (or selectively remove).
- POI actors: select the `/MapBlockout/POIs/` folder in the World Outliner, delete.
- Foliage: `UFoliageService.remove_foliage_in_radius()` (or `remove_all_foliage_of_type()` for the specific types you added).
- Painted layers: `ULandscapeService.fill_layer(landscape_label, "Crop", 0.0)` resets the layer to zero before re-painting.

The skeleton implementations all return `success=false` with `"not yet implemented"`. The contributor wires in the actual calls when porting.
