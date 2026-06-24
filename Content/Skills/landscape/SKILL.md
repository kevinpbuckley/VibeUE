---
name: landscape
display_name: Landscape Terrain
description: Create and edit landscape terrain — heightmaps, sculpting, paint layers, terrain analysis, mesh projection, and procedural features like mountains/valleys/craters (LandscapeService). Use when the user asks to create a landscape, sculpt/raise/lower terrain, import or export a heightmap, paint terrain layers, or add procedural terrain features. For terrain materials load landscape-materials; for real-world heightmaps load terrain-data.
vibeue_classes:
  - LandscapeService
unreal_classes:
  - Landscape
  - LandscapeProxy
  - LandscapeInfo
  - LandscapeLayerInfoObject
  - LandscapeGrassType
  - GrassVariety
keywords:
  - landscape
  - terrain
  - heightmap
  - sculpt
  - paint
  - layer
  - topography
  - grass
  - grass type
  - LGT
  - foliage
  - vegetation
  - mountain
  - valley
  - ridge
  - plateau
  - crater
  - terrace
  - erosion
  - slope
  - terrain analysis
  - mesh projection
  - line trace
  - flat area
  - blend terrain
  - procedural terrain
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "landscape-and-foliage"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Landscape Terrain Skill

## Sub-docs available

This skill is split into the lean index (you are reading it) plus sibling sub-docs. Skills
load via the engine's `AgentSkillToolset` (`ListSkills` / `GetSkills`) — request
`landscape/<section>` through `GetSkills`, or read the sibling `*.md` file (it sits next to
this `SKILL.md` on disk) directly:

| Sub-doc | When to load |
|---------|--------------|
| `return-types.md` | Exact property names of `LandscapeCreateResult`, `LandscapeInfo_Custom`, `LandscapeLayerInfo_Custom`, `LandscapeSplinePointInfo`, `LandscapeSplineSegmentInfo`, `LandscapeSplineMeshEntryInfo`, plus the `create_landscape` signature. |
| `workflows-creation.md` | Creating landscapes, getting info, importing/exporting heightmaps, recreating splines, copying terrain/paint layers between landscapes, calculating side-by-side offsets, reading height data in a region. |
| `workflows-editing.md` | Sculpting, direct region editing, procedural noise, painting layers, setting material, querying terrain, LandscapeGrassType creation/cloning, full landscape cloning, existence checks, heightmap sizing utilities. |
| `intelligent-sculpting.md` | v3 semantic features (mountain/valley/ridge/plateau/crater/terraces/erosion/blend), terrain analysis (slope/normal/flat areas), mesh projection, batch line traces, complete v3 workflow example. |

## Critical Rules

### Valid Quad Sizes

QuadsPerSection must be one of: **7, 15, 31, 63, 127, 255**
SectionsPerComponent must be **1 or 2**

Common setup: `QuadsPerSection=63, SectionsPerComponent=1, ComponentCount=8x8`

### ⚠️ Performance: Landscape Creation Timeout

Creating landscapes with many components is SLOW. Python execution has a 30-second timeout.

**Safe configurations (under 30s):**
- `ComponentCount=4x4` (253x253 resolution) — safe for throwaway/test terrains
- `ComponentCount=8x8` (505x505 resolution) — **can exceed 30s** in a populated level (measured 67s in a level that already had another landscape); safe only in near-empty levels

**Will TIMEOUT (avoid):**
- `ComponentCount=16x16` or larger in a populated level
- `ComponentCount=36x36` or larger — too many components, takes minutes
- `ComponentCount=72x72` — definitely exceeds timeout

**⚠️ A timeout does NOT cancel the operation.** `PYTHON_EXECUTION_TIMEOUT` means the tool stopped waiting — the create keeps running on the game thread and usually completes. **Never blindly retry**: you'd stack a second landscape with the same label (then `delete_landscape` removes only the first match and `landscape_exists` stays true, which looks like a delete failure). After a timeout, check `landscape_exists(label)` first. `create_landscape` also refuses to create a landscape whose label already exists.

### ⚠️ Heightmap Import: Resolution MUST Exactly Match

`import_heightmap()` requires the heightmap file resolution to **exactly match** the landscape resolution. **There is NO automatic scaling.** A size mismatch will fail or produce flat/corrupt terrain.

If the target landscape label does not exist, `import_heightmap()` now auto-creates a matching landscape at world origin with default scale `(100,100,100)` before importing. For real-world terrain workflows, explicitly creating the landscape first is still recommended so you can control XY/Z scale.

**Landscape resolution formula:** `Resolution = (ComponentCount × QuadsPerSection × SectionsPerComponent) + 1`

**Safe performant configs (common resolutions):**

| Components | Quads | Sections | Resolution | km at scale=100 | Notes |
|-----------|-------|----------|------------|-----------------|-------|
| 8×8       | 63    | 1        | **505×505**    | ~0.5 km | Fast, good for prototypes |
| 8×8       | 63    | 2        | **1009×1009**  | ~1.0 km | Good balance of detail/speed |
| 16×16     | 63    | 1        | **1009×1009**  | ~1.0 km | Same resolution, more LOD components |
| 8×8       | 127   | 1        | **1017×1017**  | ~1.0 km | Larger quads, fewer components |
| 16×16     | 63    | 2        | **2017×2017**  | ~2.0 km | Epic recommended for medium landscapes |
| 32×32     | 63    | 1        | **2017×2017**  | ~2.0 km | Same resolution, more LOD components |
| 8×8       | 127   | 2        | **2033×2033**  | ~2.0 km | High detail, fewer components |
| 16×16     | 127   | 1        | **2033×2033**  | ~2.0 km | High detail, more components |
| 32×32     | 63    | 2        | **4033×4033**  | ~4.0 km | Large open world |
| 32×32     | 127   | 2        | **8129×8129**  | ~8.1 km | Max single tile |

**⚠️ 1081×1081 is NOT a valid performant config!** The only config producing 1081 is `36×36 components, 15 quads, 2 sections` — which has 1296 components and **will timeout**. Never request or assume 1081.

### Heightmap ↔ Landscape Sizing Utilities

Four Python utility functions help match heightmaps to landscapes:

| Function | Purpose |
|----------|---------|
| `get_heightmap_dimensions(file_path)` | Read width/height/bitdepth of a PNG or RAW heightmap file |
| `resize_heightmap(source, width, height, output)` | Bilinear resample a 16-bit heightmap to new dimensions |
| `calculate_landscape_resolution(cx, cy, quads, sections)` | Compute resolution from landscape config parameters |
| `find_landscape_config_for_resolution(width, height)` | Find the best landscape config that matches a given resolution |

### ✅ Recommended Workflow: Request Matching Resolution

**BEST approach — request the heightmap at the exact resolution you need:**

1. Decide landscape config (e.g., 8×8 components, 63 quads, 2 sections)
2. Calculate resolution: `(8 × 63 × 2) + 1 = 1009`
3. Use `terrain_data generate_heightmap` with `resolution=1009` and `map_size=N`
   - `map_size` controls how many real-world km are captured (default 17.28)
   - Smaller `map_size` = more detail for a specific feature (e.g., 2-5 km for a single mountain)
   - Larger `map_size` = wider area with less per-pixel detail (e.g., 10-20 km for a region)
4. Create landscape with matching config
5. Import heightmap — sizes match perfectly

### ✅ Fallback Workflow: Resize After Download

If you already have a heightmap at a non-matching resolution:

1. Check dimensions: `get_heightmap_dimensions(file_path)`
2. Either:
   - Find a landscape config that matches: `find_landscape_config_for_resolution(width, height)`
   - Or resize the heightmap to match your landscape: `resize_heightmap(source, target_w, target_h)`
3. Create landscape / import resized heightmap

### Landscape Scale

- Default scale `(100, 100, 100)` = 1 meter per unit
- Larger Z scale = taller terrain range (e.g., Z=200 doubles height range)

**⚠️ When importing real-world terrain from `terrain_data`, ALWAYS calculate Z scale:**
```
z_scale = 20000 / height_scale
```
Do NOT hardcode Z scale. The `height_scale` from `preview_elevation` determines the correct Z scale.

### Height Limits (uint16 Saturation)

The heightmap is uint16 (0–65535, midpoint 32768). Maximum world height depends on Z scale:

| Z Scale | Max Height (world units) | Min Height (world units) | Total Range |
|---------|--------------------------|--------------------------|-------------|
| 100     | ~25,599                  | ~-25,600                 | ~51,200     |
| 200     | ~51,198                  | ~-51,200                 | ~102,400    |
| 50      | ~12,800                  | ~-12,800                 | ~25,600     |

Formula: `MaxHeight = (65535 - 32768) × (1/128) × ZScale`

When sculpting pushes vertices to 0 or 65535, a saturation warning is logged. To increase range:
- Increase landscape Z scale (halves vertical resolution)
- Offset landscape Z location downward to use the full ±range

### World Coordinates vs Landscape Coordinates

- **Most methods** accept world coordinates (WorldX, WorldY)
- **get_height_in_region** and **set_height_in_region** use landscape-local vertex indices
- Heights in `set_height_in_region` are world-space Z values
- Heights returned by `get_height_in_region` are world-space Z values

### ⚠️ GrassVariety Struct Properties Are Read-Only via Direct Assignment

`GrassVariety` (and its nested structs like `PerPlatformFloat`, `FloatInterval`, etc.) **cannot** be set via direct attribute assignment. You MUST use `set_editor_property()` for all properties.

```python
# WRONG - raises "Property is read-only and cannot be set"
nv = unreal.GrassVariety()
nv.affect_distance_field_lighting = True
nv.grass_density.default = 50.0

# CORRECT - use set_editor_property and construct new struct instances
nv = unreal.GrassVariety()
nv.set_editor_property('affect_distance_field_lighting', True)
nv.set_editor_property('grass_density', unreal.PerPlatformFloat(default=50.0))
nv.set_editor_property('scale_x', unreal.FloatInterval(min=1.0, max=3.0))
```

This applies to ALL nested structs: `PerPlatformFloat`, `PerPlatformInt`, `PerQualityLevelFloat`, `PerQualityLevelInt`, `FloatInterval`, `LightingChannels`. Always construct a **new** struct instance with keyword args.

### ⚠️ Creating LandscapeGrassType Assets Requires AssetTools + Factory

LandscapeGrassType assets are NOT created via LandscapeService. Use `AssetToolsHelpers` with `LandscapeGrassTypeFactory`:

```python
# WRONG - no such method exists
unreal.LandscapeService.create_grass_type("LGT_MyGrass", "/Game/Grass")

# CORRECT - use AssetTools + Factory
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.LandscapeGrassTypeFactory()
new_asset = asset_tools.create_asset("LGT_MyGrass", "/Game/Grass", unreal.LandscapeGrassType, factory)
```

### ⚠️ Use LandscapeMaterialService for Landscape Materials - NOT MaterialService

- **CORRECT**: `unreal.LandscapeMaterialService.create_landscape_material("M_Terrain", "/Game/Mats")`
- **WRONG**: `unreal.MaterialService.create_material("M_Terrain", "/Game/Mats")` → causes `TypeError: Nativize: Cannot nativize 'MaterialCreateResult' as 'String'`

`MaterialService` is for generic materials. Landscape materials MUST use `LandscapeMaterialService`.

### Layer Info Objects Required

Every paint layer needs a `ULandscapeLayerInfoObject` asset:
1. Create with `LandscapeMaterialService.create_layer_info_object()`
2. **ALWAYS store and use `.asset_path` from the result** — never guess paths
3. Naming convention is `LI_<LayerName>` (e.g., `LI_Grass`, `LI_Rock`) — NOT `Grass_LayerInfo`
4. Then add to landscape with `LandscapeService.add_layer(landscape_label, info.asset_path)`

### Material Must Be Compiled First

After creating/changing a landscape material, compile it before assigning to the landscape.

### ⚠️ Split Material Operations Into Separate Code Blocks

`compile_material()` can take minutes for complex landscape materials. **NEVER** combine material creation + compile + layer info creation in one code block — it WILL timeout.

Do this in separate steps:
1. **Block 1**: Create material, add blend nodes, add layers, connect, save
2. **Block 2**: `compile_material()` (this is slow)
3. **Block 3**: Create layer info objects, assign to landscape

---

### ⚠️ Common Mistakes to Avoid

| WRONG | CORRECT |
|-------|---------|
| `create_landscape(..., actor_label="X")` | `create_landscape(..., landscape_label="X")` |
| `result.actor_path` | `result.actor_label` (no actor_path exists) |
| `info.success` | Check `info.actor_label` (no success field on info) |
| `info.num_sections` | `info.num_subsections` |
| `info.quads_per_section` | `info.subsection_size_quads` |
| `MaterialService.create_material()` for landscapes | `LandscapeMaterialService.create_landscape_material()` |
| Guessing layer info path `Grass_LayerInfo` | Use `create_layer_info_object().asset_path` → `LI_Grass` |
| `segment.start_control_point_index` | `segment.start_point_index` (spline segment) |
| `segment.end_control_point_index` | `segment.end_point_index` (spline segment) |
| `layer.name` on list_layers result | `layer.layer_name` (LandscapeLayerInfo_Custom property) |
| `LandscapeService.create_spline_control_point(...)` | `LandscapeService.create_spline_point(...)` (no "control" in name) |
| `LandscapeMaterialService.add_material_layer(...)` | `LandscapeMaterialService.add_layer_to_blend_node(...)` |
| `mat_path = create_landscape_material(...)` as string | Returns `LandscapeMaterialCreateResult` — use `result.asset_path` |
| `connect_spline_points(..., tangent_length=25.0)` ignoring negative source | Pass `tangent_length=seg.start_tangent_length, end_tangent_length=seg.end_tangent_length` — **both** tangents are needed for exact shape |
| Only passing `tangent_length` to `connect_spline_points` | Must also pass `end_tangent_length=seg.end_tangent_length` — end tangent is typically negative (UE convention). Omitting it defaults to `-start_tangent` which is usually correct for NEW splines, but when copying, always pass both explicitly |
| `create_spline_point("L", location=vec)` | Parameter is `world_location`, NOT `location`: `create_spline_point("L", world_location=vec)` |
| `modify_spline_point("L", 0, location=vec)` | Parameter is `world_location`, NOT `location`: `modify_spline_point("L", 0, world_location=vec)` |
| `modify_spline_point(...)` with wrong rotation | Use `auto_calc_rotation=False, rotation=pt.rotation` to set exact rotation from get_spline_info |
| Setting rotation BEFORE `connect_spline_points` | `connect_spline_points` triggers auto-calc rotation — set rotations AFTER all segments are connected |
| `pt.paint_layer_name` on LandscapeSplinePointInfo | Property is `pt.layer_name` (NOT `paint_layer_name`) — `paint_layer_name` is the **parameter** name in `create_spline_point` |
| Reading weights right after painting and getting 0.0 | Weights are written to edit layer — reads use same path |
| `spline_info.points` on LandscapeSplineInfo | Property is `spline_info.control_points` (**NOT** `points`) |
| Not setting spline segment meshes after connecting | Use `set_spline_segment_meshes()` to assign mesh entries (e.g. SM_River) — splines are green without meshes |
| Forgetting to set control point meshes | Use `set_spline_point_mesh()` — control points can have their own static mesh |
| `layer.asset_path` on LandscapeLayerInfo_Custom | Property is `layer.layer_info_path` (**NOT** `asset_path`) |
| Guessing random offset like 110000 for side-by-side | Calculate: `offset = (resolution - 1) * scale.x` e.g. (1009-1)*100 = 100800 |
| Using `FoliageService.clear_all_foliage()` thinking it only clears one landscape | `clear_all_foliage()` removes ALL foliage from the ENTIRE level — use `remove_foliage_in_radius()` or `remove_all_foliage_of_type()` to target specific areas |
| Manually scattering foliage to replicate a landscape that uses LandscapeGrassType | If foliage is procedural (from LandscapeGrassType on the material), just copy paint layers — foliage auto-generates from weights. Check if `list_foliage_types()` returns 0; if so, foliage is procedural. |
| Using `terrain_data` without `resolution` and getting 1081×1081 | Always pass `resolution=N` matching your landscape. Common values: 505, 1009, 1017, 2033 |
| Assuming 1081×1081 heightmap will fit any landscape | 1081 requires 36×36 components which WILL timeout. Use `resolution=1009` (8×8, 63q, 2s) instead |
| Importing a heightmap without checking dimensions | Always call `get_heightmap_dimensions()` first, then `resize_heightmap()` if sizes don't match |
| Creating landscape then downloading heightmap (wrong order) | Decide config → calculate resolution → download at that resolution → create landscape → import |
| `info.scale_x` on LandscapeInfo_Custom | `info.scale.x` — `scale`, `location`, `rotation` are Vector/Rotator structs, not flattened floats |
| Assuming a landscape is centered on its location | It spans **+X/+Y from its location** (corner-anchored): bounds = `location` to `location + (resolution-1)*scale`. A landscape created at (0,0) with 505 res @ scale 100 covers (0,0)–(50400,50400); its center is (25200,25200) |
| `create_layer_info_object("Grass", "/Game/X", "LI_Grass")` | Third param is a **bool**: `create_layer_info_object(layer_name, destination_path, is_weight_blended=True)` — passing a string asset name causes `TypeError: Cannot nativize 'str' as 'bool'` |
| Retrying `create_landscape` after `PYTHON_EXECUTION_TIMEOUT` | The first call usually still completes in the background — check `landscape_exists(label)` before retrying (duplicate labels now return an error) |
| `result.location` on LandscapeCreateResult | Result has ONLY `success`, `actor_label`, `error_message` — get location via `get_landscape_info(label).location` |
| `result.resolution_x` / `.resolution_y` on HeightmapImportResult | The field is `result.resolution` — a **string** like `"1009x1009"`, not separate ints. Split it if you need numbers. |
| Treating `get_height_at_location()` as a float | It returns a `LandscapeHeightSample` struct — use `.height` (float) and `.valid` (bool); `float(sample)` / `f"{sample}"` raise TypeError |
| `delete_landscape(label)` to clear a World-Partition landscape | It returns `False` for `LandscapeStreamingProxy` actors. To wipe a level clean, also destroy proxies: `[s.destroy_actor(a) for a in EditorActorSubsystem.get_all_level_actors() if isinstance(a, unreal.LandscapeStreamingProxy)]` (a `delete_all_landscapes()` batch would be cleaner — not yet available) |
| `info.component_count_x` / `info.component_count_y` | `info.num_components` is the TOTAL count (e.g. 64 for 8×8) — there are no per-axis fields |
| `info.rotation.x` (Rotator) | Rotator fields are `.roll` / `.pitch` / `.yaw` — there is no `.x/.y/.z` on Rotator (only Vector has those) |

---

## Related Skills

| Task | Skills to Load |
|------|---------------|
| Sculpt terrain only | `landscape` |
| Simple painted material (LayerBlend) | `landscape` + `landscape-materials` |
| Production auto-material (material functions, RVT, instances) | `landscape` + `landscape-auto-material` |
| Material instance / biome configuration | `landscape-auto-material` |

- **landscape-materials**: Simple landscape materials with `LandscapeLayerBlend` nodes. Good for prototyping with 2-5 layers.
- **landscape-auto-material**: Production-quality auto-materials using material functions, Runtime Virtual Textures, and material instances (Real_Landscape paradigm). Good for shipping quality.

When you need to create or modify the material applied to a landscape, load the appropriate
material skill through `GetSkills` (AgentSkillToolset):

- For simple materials: load `landscape-materials`
- For production auto-materials: load `landscape-auto-material`

> **NOTE**: For landscape material, texture, and layer blending operations, use `LandscapeMaterialService` (load the `landscape-materials` or `landscape-auto-material` skill).

## Sample scripts (run via `execute_python_code`)

- **`scripts/sculpt_terrain.txt`** — create a landscape and sculpt procedural mountain/valley features.
