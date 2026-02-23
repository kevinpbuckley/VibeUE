---
name: landscape
display_name: Landscape Terrain
description: Create and edit landscape terrain, heightmaps, sculpting, and paint layers using LandscapeService
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
---

# Landscape Terrain Skill

## Critical Rules

### Valid Quad Sizes

QuadsPerSection must be one of: **7, 15, 31, 63, 127, 255**
SectionsPerComponent must be **1 or 2**

Common setup: `QuadsPerSection=63, SectionsPerComponent=1, ComponentCount=8x8`

### Landscape Scale

- Default scale `(100, 100, 100)` = 1 meter per unit
- Larger Z scale = taller terrain range (e.g., Z=200 doubles height range)

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

## Return Types - EXACT Properties

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

### LandscapeCreateResult (from create_landscape)

| Property | Type | Description |
|----------|------|-------------|
| `success` | bool | Whether creation succeeded |
| `actor_label` | str | The label assigned to the landscape |
| `error_message` | str | Error details if failed |

**No other properties exist.** No `actor_path`, no `actor_name`.

### LandscapeInfo_Custom (from get_landscape_info)

| Property | Type | Description |
|----------|------|-------------|
| `actor_name` | str | Internal actor name |
| `actor_label` | str | Display label |
| `location` | Vector | World location |
| `rotation` | Rotator | World rotation |
| `scale` | Vector | Scale (e.g. 100,100,100) |
| `component_size_quads` | int | Quads per component |
| `subsection_size_quads` | int | Quads per subsection |
| `num_subsections` | int | Sections per component |
| `num_components` | int | Total component count |
| `resolution_x` | int | Total X resolution |
| `resolution_y` | int | Total Y resolution |
| `material_path` | str | Assigned material path |
| `layers` | Array | List of LandscapeLayerInfo_Custom |

**No `success` field.** Check `actor_label` to verify data was returned.

### LandscapeLayerInfo_Custom (from get_landscape_info or list_layers)

| Property | Type | Description |
|----------|------|-------------|
| `layer_name` | str | Name of the paint layer (e.g. "L1", "Grass") |
| `layer_info_path` | str | Asset path of the layer info object (**NOT** `asset_path`) |
| `is_weight_blended` | bool | Whether the layer uses weight blending |

### create_landscape Signature

```
create_landscape(location, rotation, scale,
    sections_per_component=1, quads_per_section=63,
    component_count_x=8, component_count_y=8,
    landscape_label="") -> LandscapeCreateResult
```

The label parameter is `landscape_label`, NOT `actor_label`.

### LandscapeSplinePointInfo (from get_spline_info)

| Property | Type | Description |
|----------|------|-------------|
| `point_index` | int | Index of this control point |
| `location` | Vector | World-space position |
| `rotation` | Rotator | Control point rotation (tangent direction) |
| `width` | float | Half-width of spline influence |
| `side_falloff` | float | Side falloff distance |
| `end_falloff` | float | End tip falloff distance |
| `layer_name` | str | Paint layer applied under spline (**NOT** `paint_layer_name`) |
| `raise_terrain` | bool | Whether terrain is raised to spline |
| `lower_terrain` | bool | Whether terrain is lowered to spline |
| `mesh_path` | str | Static mesh assigned to this control point (empty = none) |
| `mesh_scale` | Vector | Scale of the control point mesh |
| `segment_mesh_offset` | float | Offset for mesh at segment connections |

### LandscapeSplineSegmentInfo (from get_spline_info)

| Property | Type | Description |
|----------|------|-------------|
| `segment_index` | int | Index of this segment |
| `start_point_index` | int | Index of start control point (**NOT** `start_control_point_index`) |
| `end_point_index` | int | Index of end control point (**NOT** `end_control_point_index`) |
| `start_tangent_length` | float | Start tangent arm length |
| `end_tangent_length` | float | End tangent arm length |
| `layer_name` | str | Layer painted under segment |
| `raise_terrain` | bool | Raise terrain under segment |
| `lower_terrain` | bool | Lower terrain under segment |
| `spline_meshes` | Array[LandscapeSplineMeshEntryInfo] | Mesh entries assigned to this segment |

### LandscapeSplineMeshEntryInfo (mesh entry in segment spline_meshes)

| Property | Type | Description |
|----------|------|-------------|
| `mesh_path` | str | Asset path of the static mesh (e.g. `/Game/.../SM_River.SM_River`) |
| `scale` | Vector | XYZ scale of the mesh along the spline |
| `scale_to_width` | bool | Whether mesh scales to match spline width |
| `material_override_paths` | Array[str] | Asset paths of material overrides (empty = use mesh defaults) |
| `center_adjust` | Vector2D | XY offset to center the mesh on the spline |
| `forward_axis` | int | Spline mesh forward axis: 0=X, 1=Y, 2=Z |
| `up_axis` | int | Spline mesh up axis: 0=X, 1=Y, 2=Z |

---

## Workflows

### Create Basic Landscape

```python
import unreal

# IMPORTANT: The label kwarg is 'landscape_label', NOT 'actor_label'
result = unreal.LandscapeService.create_landscape(
    location=unreal.Vector(0, 0, 0),
    rotation=unreal.Rotator(0, 0, 0),
    scale=unreal.Vector(100, 100, 100),
    sections_per_component=1,
    quads_per_section=63,
    component_count_x=8,
    component_count_y=8,
    landscape_label="MyTerrain"  # NOT actor_label
)
# LandscapeCreateResult has ONLY: success, actor_label, error_message
if result.success:
    print(f"Created: {result.actor_label}")
else:
    print(f"Failed: {result.error_message}")
```

### Get Landscape Info

```python
import unreal

# get_landscape_info returns LandscapeInfo_Custom (NO success field)
info = unreal.LandscapeService.get_landscape_info("MyTerrain")
# Check actor_label to verify it found the landscape
if info.actor_label:
    print(f"Label: {info.actor_label}")
    print(f"Scale: {info.scale}")
    print(f"Components: {info.num_components}")
    print(f"Resolution: {info.resolution_x}x{info.resolution_y}")
    print(f"Subsections: {info.num_subsections}")
    print(f"Subsection Size: {info.subsection_size_quads}")
    print(f"Material: {info.material_path}")
```

### Import Heightmap

```python
import unreal

# RAW file must be 16-bit, matching landscape resolution
# After import, the landscape visually updates immediately (collision + render rebuilt)
success = unreal.LandscapeService.import_heightmap("MyTerrain", "C:/Heightmaps/terrain.raw")
if success:
    print("Heightmap imported successfully")
```

### Export Heightmap

```python
import unreal

unreal.LandscapeService.export_heightmap("MyTerrain", "C:/Heightmaps/terrain_export.raw")
```

### Recreate Splines from One Landscape to Another

```python
import unreal

svc = unreal.LandscapeService

# 1. Get source spline data
src = svc.get_spline_info("SourceLandscape")

# 2. Create control points on destination (use pt.layer_name, NOT pt.paint_layer_name)
#    IMPORTANT: keyword is world_location=, NOT location=
point_mapping = {}
for pt in src.control_points:
    res = svc.create_spline_point(
        "DestLandscape", world_location=pt.location,
        width=pt.width, side_falloff=pt.side_falloff, end_falloff=pt.end_falloff,
        paint_layer_name=pt.layer_name if pt.layer_name else "",
        raise_terrain=pt.raise_terrain, lower_terrain=pt.lower_terrain
    )
    if res.success:
        point_mapping[pt.point_index] = res.point_index

# 3. Connect segments with EXACT tangent lengths (BOTH start and end)
for seg in src.segments:
    if seg.start_point_index in point_mapping and seg.end_point_index in point_mapping:
        svc.connect_spline_points(
            "DestLandscape",
            point_mapping[seg.start_point_index],
            point_mapping[seg.end_point_index],
            tangent_length=seg.start_tangent_length,
            end_tangent_length=seg.end_tangent_length,  # MUST pass both!
            paint_layer_name=seg.layer_name if seg.layer_name else "",
            raise_terrain=seg.raise_terrain, lower_terrain=seg.lower_terrain
        )

# 4. Set EXACT rotations AFTER all connections
#    CRITICAL: connect_spline_points triggers auto-calc rotation,
#    so rotations must be applied LAST to stick
#    IMPORTANT: keyword is world_location=, NOT location=
for pt in src.control_points:
    if pt.point_index in point_mapping:
        svc.modify_spline_point(
            "DestLandscape", point_mapping[pt.point_index],
            world_location=pt.location,
            rotation=pt.rotation, auto_calc_rotation=False
        )

# 5. Copy spline meshes from source segments
for seg in src.segments:
    if seg.spline_meshes and len(seg.spline_meshes) > 0:
        svc.set_spline_segment_meshes(
            "DestLandscape", seg.segment_index, seg.spline_meshes
        )

# 6. Copy control point meshes from source points
for pt in src.control_points:
    if pt.mesh_path:
        svc.set_spline_point_mesh(
            "DestLandscape", point_mapping[pt.point_index],
            pt.mesh_path, pt.mesh_scale, pt.segment_mesh_offset
        )

# 7. Apply to deform terrain
svc.apply_splines_to_landscape("DestLandscape")
```

> **Order matters:** Create points → Connect segments → Set rotations → Set meshes → Apply.
> `connect_spline_points` overwrites control point rotations via auto-calc.
> Always set explicit rotations as the LAST step before apply.

### Copy Terrain Between Landscapes

```python
import unreal

# Method 1: Using get_height_in_region / set_height_in_region (no file I/O)
info = unreal.LandscapeService.get_landscape_info("SourceLandscape")
res_x = info.resolution_x
res_y = info.resolution_y

# Read all heights as world-space Z values (landscape-local vertex coords)
heights = unreal.LandscapeService.get_height_in_region("SourceLandscape", 0, 0, res_x, res_y)
if len(heights) > 0:
    # Write to destination (must have same resolution)
    unreal.LandscapeService.set_height_in_region("DestLandscape", 0, 0, res_x, res_y, heights)

# Method 2: Using export/import via temp file
import os
temp_path = os.path.join(unreal.Paths.project_saved_dir(), "temp_heightmap.raw")
unreal.LandscapeService.export_heightmap("SourceLandscape", temp_path)
unreal.LandscapeService.import_heightmap("DestLandscape", temp_path)
os.remove(temp_path)  # Clean up
```

### Copy Paint Layers Between Landscapes

```python
import unreal

src = "SourceLandscape"
dst = "DestLandscape"
info = unreal.LandscapeService.get_landscape_info(src)
layers = unreal.LandscapeService.list_layers(src)

# 1. Add all layers to destination (uses layer_info_path, NOT asset_path)
for layer in layers:
    unreal.LandscapeService.add_layer(dst, layer.layer_info_path)

# 2. Copy weights for each layer
for layer in layers:
    weights = unreal.LandscapeService.get_weights_in_region(
        src, layer.layer_name, 0, 0, info.resolution_x, info.resolution_y)
    if weights and sum(weights) > 0:
        unreal.LandscapeService.set_weights_in_region(
            dst, layer.layer_name, 0, 0, info.resolution_x, info.resolution_y, weights)
        print(f"Copied weights for {layer.layer_name}")
```

### Calculate Landscape Offset for Side-by-Side

```python
import unreal

# Correct way to position a duplicate landscape next to the source:
info = unreal.LandscapeService.get_landscape_info("SourceLandscape")
# Total world size = (resolution - 1) * scale component
landscape_width = (info.resolution_x - 1) * info.scale.x  # e.g. (1009-1)*100 = 100800
new_x = info.location.x + landscape_width  # place immediately to the right
new_loc = unreal.Vector(new_x, info.location.y, info.location.z)
# Do NOT use arbitrary offsets like 110000 — calculate from actual dimensions
```

### Read Height Data in Region

```python
import unreal

# Read a 100x100 vertex region starting at vertex (0, 0)
# Returns world-space Z heights as a flat array (row-major)
heights = unreal.LandscapeService.get_height_in_region("MyTerrain", 0, 0, 100, 100)
if len(heights) > 0:
    print(f"Read {len(heights)} height values")
    print(f"First height: {heights[0]}")
    print(f"Last height: {heights[-1]}")

# Read the ENTIRE landscape heightmap
info = unreal.LandscapeService.get_landscape_info("MyTerrain")
all_heights = unreal.LandscapeService.get_height_in_region(
    "MyTerrain", 0, 0, info.resolution_x, info.resolution_y)
```

### Sculpt Terrain

```python
import unreal

# Raise terrain by 500 world units at center (with falloff)
unreal.LandscapeService.sculpt_at_location("MyTerrain", 500.0, 500.0, 1000.0, 500.0, "Smooth")

# Lower terrain (negative height delta)
unreal.LandscapeService.sculpt_at_location("MyTerrain", 500.0, 500.0, 1000.0, -300.0, "Linear")

# Create a tall mountain peak (raise by 5000 units)
unreal.LandscapeService.sculpt_at_location("MyTerrain", 0.0, 0.0, 5000.0, 5000.0, "Smooth")

# Flatten to specific height (with optional falloff type)
unreal.LandscapeService.flatten_at_location("MyTerrain", 500.0, 500.0, 2000.0, 100.0, 1.0)
unreal.LandscapeService.flatten_at_location("MyTerrain", 500.0, 500.0, 2000.0, 100.0, 1.0, "Linear")

# Smooth terrain — uses adaptive Gaussian blur
# Higher strength = larger kernel = stronger smoothing across cliffs
unreal.LandscapeService.smooth_at_location("MyTerrain", 500.0, 500.0, 1500.0, 0.5)
unreal.LandscapeService.smooth_at_location("MyTerrain", 500.0, 500.0, 1500.0, 1.0, "Linear")
```

> **Note on sculpt saturation:** When vertices hit the uint16 height limit (0 or 65535),
> a warning is logged. With Z-scale 100, max height is ~25,599 world units.

### Direct Region Editing

```python
import unreal

# Raise a rectangular region by 1000 world units (hard edges)
unreal.LandscapeService.raise_lower_region("MyTerrain", 0.0, 0.0, 5000.0, 5000.0, 1000.0)

# Raise with smooth falloff edges (2000 unit transition band)
unreal.LandscapeService.raise_lower_region("MyTerrain", 0.0, 0.0, 5000.0, 5000.0, 1000.0, 2000.0)

# Lower a region with falloff
unreal.LandscapeService.raise_lower_region("MyTerrain", 5000.0, 5000.0, 3000.0, 3000.0, -500.0, 1000.0)
```

> **FalloffWidth parameter:** Adds a cosine-interpolated transition band around rectangle edges.
> Without it (default 0), edges are vertical cliffs. A value of 2000 creates a smooth 2000-unit
> ramp from full delta to zero. **Always use FalloffWidth for natural-looking terrain.**

### Apply Procedural Noise

`apply_noise` now returns an `FLandscapeNoiseResult` struct with operation statistics:

| Property | Type | Description |
|----------|------|-------------|
| `success` | bool | Whether noise was applied |
| `min_delta_applied` | float | Smallest height change (world units) |
| `max_delta_applied` | float | Largest height change (world units) |
| `vertices_modified` | int | Number of vertices changed |
| `saturated_vertices` | int | Vertices that hit uint16 height limit |
| `error_message` | str | Error details if failed |

```python
import unreal

# Add natural terrain variation with 4 octaves (default)
result = unreal.LandscapeService.apply_noise("MyTerrain", 0.0, 0.0, 10000.0, 500.0, 0.005, 42)
if result.success:
    print(f"Delta range: [{result.min_delta_applied:.1f}, {result.max_delta_applied:.1f}]")
    print(f"Vertices: {result.vertices_modified}, Saturated: {result.saturated_vertices}")

# Fine detail noise with more octaves (6) for extra detail
result = unreal.LandscapeService.apply_noise("MyTerrain", 0.0, 0.0, 8000.0, 100.0, 0.01, 123, 6)

# Large rolling hills with fewer octaves (2) for smooth shapes
result = unreal.LandscapeService.apply_noise("MyTerrain", 0.0, 0.0, 20000.0, 2000.0, 0.001, 7, 2)
```

> **Octaves parameter (1-8):** Controls detail layers. 1 = smooth rolling hills,
> 4 = natural default, 8 = maximum fractal detail. Clamped to [1, 8].

### Paint Layer

```python
import unreal

# 1. Create layer info object — ALWAYS use .asset_path, never guess the path
grass_info = unreal.LandscapeMaterialService.create_layer_info_object("Grass", "/Game/Landscape")
# grass_info.asset_path will be "/Game/Landscape/LI_Grass" (NOT "Grass_LayerInfo")

# 2. Add layer to landscape using the EXACT path from step 1
unreal.LandscapeService.add_layer("MyTerrain", grass_info.asset_path)

# 3. Paint at location
unreal.LandscapeService.paint_layer_at_location("MyTerrain", "Grass", 500.0, 500.0, 2000.0, 1.0)
```

### Set Landscape Material

```python
import unreal

unreal.LandscapeService.set_landscape_material("MyTerrain", "/Game/Materials/M_Terrain")
```

### Query Terrain Info

```python
import unreal

# List all landscapes
landscapes = unreal.LandscapeService.list_landscapes()
for ls in landscapes:
    print(f"{ls.actor_label}: {ls.num_components} components, resolution {ls.resolution_x}x{ls.resolution_y}")

# Get height at location
sample = unreal.LandscapeService.get_height_at_location("MyTerrain", 500.0, 500.0)
if sample.valid:
    print(f"Height: {sample.height}")

# Get layer weights
weights = unreal.LandscapeService.get_layer_weights_at_location("MyTerrain", 500.0, 500.0)
for w in weights:
    print(f"{w.layer_name}: {w.weight}")
```

### Create LandscapeGrassType Asset

```python
import unreal

# Create a new LandscapeGrassType asset using AssetTools + Factory
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.LandscapeGrassTypeFactory()
new_lgt = asset_tools.create_asset("LGT_MyGrass", "/Game/Landscape", unreal.LandscapeGrassType, factory)

# Set top-level properties
new_lgt.set_editor_property('enable_density_scaling', True)

# Build grass varieties array
varieties = unreal.Array(unreal.GrassVariety)

v = unreal.GrassVariety()
v.set_editor_property('grass_mesh', unreal.load_asset("/Game/Meshes/SM_Grass_01"))
v.set_editor_property('grass_density', unreal.PerPlatformFloat(default=100.0))
v.set_editor_property('grass_density_quality', unreal.PerQualityLevelFloat(default=400.0))
v.set_editor_property('start_cull_distance', unreal.PerPlatformInt(default=3000))
v.set_editor_property('start_cull_distance_quality', unreal.PerQualityLevelInt(default=10000))
v.set_editor_property('end_cull_distance', unreal.PerPlatformInt(default=3000))
v.set_editor_property('end_cull_distance_quality', unreal.PerQualityLevelInt(default=10000))
v.set_editor_property('scale_x', unreal.FloatInterval(min=1.0, max=2.0))
v.set_editor_property('scale_y', unreal.FloatInterval(min=1.0, max=1.0))
v.set_editor_property('scale_z', unreal.FloatInterval(min=1.0, max=1.0))
v.set_editor_property('allowed_density_range', unreal.FloatInterval(min=0.0, max=1.0))
v.set_editor_property('lighting_channels', unreal.LightingChannels(channel0=True, channel1=False, channel2=False))
v.set_editor_property('scaling', unreal.GrassScaling.UNIFORM)
v.set_editor_property('random_rotation', True)
v.set_editor_property('align_to_surface', True)
v.set_editor_property('use_grid', True)
v.set_editor_property('placement_jitter', 1.0)
varieties.append(v)

new_lgt.set_editor_property('grass_varieties', varieties)
unreal.EditorAssetLibrary.save_asset("/Game/Landscape/LGT_MyGrass")
```

### Clone LandscapeGrassType (Read + Recreate)

To recreate a LandscapeGrassType without duplicating, read properties from source and write to a new asset:

```python
import unreal

# Load source
src = unreal.load_asset("/Game/Landscape/LGT_Grass")

# Create new asset
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.LandscapeGrassTypeFactory()
dst = asset_tools.create_asset("LGT_Grass2", "/Game/Landscape", unreal.LandscapeGrassType, factory)
dst.set_editor_property('enable_density_scaling', src.get_editor_property('enable_density_scaling'))

# Copy all varieties
src_vars = src.get_editor_property('grass_varieties')
new_vars = unreal.Array(unreal.GrassVariety)
for sv in src_vars:
    nv = unreal.GrassVariety()
    # Simple properties — MUST use set_editor_property, not direct assignment
    for prop in ['affect_distance_field_lighting', 'align_to_surface', 'align_to_triangle_normals',
                 'cast_contact_shadow', 'cast_dynamic_shadow', 'grass_mesh',
                 'keep_instance_buffer_cpu_copy', 'max_scale_weight_attenuation', 'min_lod',
                 'placement_jitter', 'random_rotation', 'receives_decals', 'scaling',
                 'shadow_cache_invalidation_behavior', 'use_grid', 'use_landscape_lightmap',
                 'weight_attenuates_max_scale']:
        nv.set_editor_property(prop, sv.get_editor_property(prop))
    # Struct properties — construct NEW instances with keyword args
    nv.set_editor_property('grass_density', unreal.PerPlatformFloat(default=sv.grass_density.default))
    nv.set_editor_property('grass_density_quality', unreal.PerQualityLevelFloat(default=sv.grass_density_quality.default))
    nv.set_editor_property('start_cull_distance', unreal.PerPlatformInt(default=sv.start_cull_distance.default))
    nv.set_editor_property('start_cull_distance_quality', unreal.PerQualityLevelInt(default=sv.start_cull_distance_quality.default))
    nv.set_editor_property('end_cull_distance', unreal.PerPlatformInt(default=sv.end_cull_distance.default))
    nv.set_editor_property('end_cull_distance_quality', unreal.PerQualityLevelInt(default=sv.end_cull_distance_quality.default))
    nv.set_editor_property('scale_x', unreal.FloatInterval(min=sv.scale_x.min, max=sv.scale_x.max))
    nv.set_editor_property('scale_y', unreal.FloatInterval(min=sv.scale_y.min, max=sv.scale_y.max))
    nv.set_editor_property('scale_z', unreal.FloatInterval(min=sv.scale_z.min, max=sv.scale_z.max))
    nv.set_editor_property('allowed_density_range', unreal.FloatInterval(min=sv.allowed_density_range.min, max=sv.allowed_density_range.max))
    nv.set_editor_property('lighting_channels', unreal.LightingChannels(
        channel0=sv.lighting_channels.channel0, channel1=sv.lighting_channels.channel1, channel2=sv.lighting_channels.channel2))
    new_vars.append(nv)

dst.set_editor_property('grass_varieties', new_vars)
unreal.EditorAssetLibrary.save_asset("/Game/Landscape/LGT_Grass2")
```

### Clone Entire Landscape (Complete Workflow)

When duplicating a landscape with all details, follow these steps IN ORDER:

**Step 1: Gather source info**
```python
import unreal

src = "SourceLandscape"
info = unreal.LandscapeService.get_landscape_info(src)
layers = unreal.LandscapeService.list_layers(src)
```

**Step 2: Calculate exact offset position**
```python
# width = (resolution - 1) * scale.x — DO NOT use arbitrary offsets
width = (info.resolution_x - 1) * info.scale.x
new_x = info.location.x + width
```

**Step 3: Create destination landscape**
```python
dst = "DestLandscape"
# Derive component counts from resolution (info has num_components total, not per-axis)
comp_size = info.subsection_size_quads * info.num_subsections
count_x = (info.resolution_x - 1) // comp_size
count_y = (info.resolution_y - 1) // comp_size

unreal.LandscapeService.create_landscape(
    location=unreal.Vector(new_x, info.location.y, info.location.z),
    rotation=info.rotation,
    scale=info.scale,
    landscape_label=dst,
    quads_per_section=info.subsection_size_quads,
    sections_per_component=info.num_subsections,
    component_count_x=count_x,
    component_count_y=count_y
)
```

**Step 4: Copy heightmap**
```python
import tempfile, os
temp = os.path.join(tempfile.gettempdir(), "terrain_copy.png")
unreal.LandscapeService.export_heightmap(src, temp)
unreal.LandscapeService.import_heightmap(dst, temp)
os.remove(temp)
```

**Step 5: Add layers and copy weight painting**
```python
for layer in layers:
    # Use layer.layer_info_path — NOT layer.asset_path
    unreal.LandscapeService.add_layer(dst, layer.layer_info_path)

for layer in layers:
    weights = unreal.LandscapeService.get_weights_in_region(
        src, layer.layer_name, 0, 0, info.resolution_x, info.resolution_y)
    if weights and sum(weights) > 0:
        unreal.LandscapeService.set_weights_in_region(
            dst, layer.layer_name, 0, 0, info.resolution_x, info.resolution_y, weights)
```

**Step 6: Copy splines (with rotations, tangents, and meshes)**
```python
spline_info = unreal.LandscapeService.get_spline_info(src)
offset = unreal.Vector(new_x - info.location.x, 0, 0)

# Create all control points
point_mapping = {}
for pt in spline_info.control_points:
    res = unreal.LandscapeService.create_spline_point(
        dst, location=pt.location + offset,
        width=pt.width, side_falloff=pt.side_falloff, end_falloff=pt.end_falloff,
        paint_layer_name=pt.layer_name if pt.layer_name else "",
        raise_terrain=pt.raise_terrain, lower_terrain=pt.lower_terrain)
    if res.success:
        point_mapping[pt.point_index] = res.point_index

# Connect segments with EXACT tangent lengths (BOTH start and end)
for seg in spline_info.segments:
    if seg.start_point_index in point_mapping and seg.end_point_index in point_mapping:
        unreal.LandscapeService.connect_spline_points(
            dst,
            point_mapping[seg.start_point_index],
            point_mapping[seg.end_point_index],
            tangent_length=seg.start_tangent_length,
            end_tangent_length=seg.end_tangent_length,  # CRITICAL: preserves exact shape
            paint_layer_name=seg.layer_name if seg.layer_name else "",
            raise_terrain=seg.raise_terrain, lower_terrain=seg.lower_terrain)

# Set rotations AFTER connecting (connect triggers auto-calc)
for pt in spline_info.control_points:
    if pt.point_index in point_mapping:
        unreal.LandscapeService.modify_spline_point(
            dst, point_mapping[pt.point_index], pt.location + offset,
            rotation=pt.rotation, auto_calc_rotation=False)

# Set segment meshes
for seg in spline_info.segments:
    if seg.spline_meshes and len(seg.spline_meshes) > 0:
        unreal.LandscapeService.set_spline_segment_meshes(
            dst, seg.segment_index, seg.spline_meshes)

unreal.LandscapeService.apply_splines(dst)
```

**Step 7: Procedural foliage will auto-generate**
```
Foliage from LandscapeGrassType (procedural) auto-generates from the landscape
material layers. Once layers and weights are copied, this foliage appears automatically.
Do NOT use FoliageService.scatter_foliage_rect for procedural foliage.
FoliageService only manages MANUALLY placed instances (e.g. individual trees).
```

### Check Existence

```python
import unreal

if not unreal.LandscapeService.landscape_exists("MyTerrain"):
    unreal.LandscapeService.create_landscape(
        location=unreal.Vector(0, 0, 0),
        rotation=unreal.Rotator(0, 0, 0),
        scale=unreal.Vector(100, 100, 100),
        landscape_label="MyTerrain"
    )

if not unreal.LandscapeService.layer_exists("MyTerrain", "Grass"):
    unreal.LandscapeService.add_layer("MyTerrain", "/Game/Landscape/LI_Grass")
```
