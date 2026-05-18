---
name: workflows-editing
description: Workflows for sculpting and editing existing terrain — sculpt_at_location, direct region editing, procedural noise, painting layers, setting landscape material, querying terrain info, creating and cloning LandscapeGrassType assets, full landscape cloning, existence checks, and heightmap sizing utilities (dimensions/resize/find-config/calculate-resolution)
---

# Workflows — Sculpt, Paint, Modify, Clone, Sizing Utilities

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

### Heightmap Sizing Utilities

#### Get Heightmap Dimensions

```python
import unreal

# Works with PNG (16-bit grayscale) and RAW (16-bit) files
dims = unreal.LandscapeService.get_heightmap_dimensions("C:/Heightmaps/terrain.png")
if dims.success:
    print(f"Size: {dims.width}x{dims.height}, Bit depth: {dims.bit_depth}")
else:
    print(f"Error: {dims.error_message}")
```

#### Calculate Landscape Resolution

```python
import unreal

# Pure math — compute what resolution a landscape config produces
info = unreal.LandscapeService.calculate_landscape_resolution(
    component_count_x=8, component_count_y=8,
    quads_per_section=63, sections_per_component=2
)
print(f"Resolution: {info.resolution_x}x{info.resolution_y}")  # 1009×1009
print(f"Total vertices: {info.total_vertices}")
print(f"Total components: {info.total_components}")
print(f"Description: {info.description}")
```

#### Find Landscape Config for Heightmap

```python
import unreal

# Given a heightmap resolution, find the landscape config that matches
config = unreal.LandscapeService.find_landscape_config_for_resolution(1009, 1009)
if config.total_components > 0:
    print(f"Match found: {config.description}")
    # Use this config when calling create_landscape
else:
    print("No exact match — resize the heightmap instead")
```

#### Resize Heightmap

```python
import unreal

# Bilinear resample — preserves terrain shape, works with 16-bit PNG and RAW
result = unreal.LandscapeService.resize_heightmap(
    "C:/Heightmaps/terrain_1081x1081.png",  # source
    1009,  # target width
    1009   # target height
    # output path auto-generated if omitted
)
if result.success:
    print(f"Resized: {result.original_dimensions} → {result.new_dimensions}")
    print(f"Output: {result.output_file}")
```

#### Complete Terrain Import Workflow (terrain_data → landscape)

```python
import unreal

# This shows the FULL recommended workflow for importing real-world terrain

# Step 1: Choose landscape config
comp_x, comp_y = 8, 8
quads = 63
sections = 2

# Step 2: Calculate resolution
res = unreal.LandscapeService.calculate_landscape_resolution(comp_x, comp_y, quads, sections)
target_res = res.resolution_x  # 1009

# Step 3: Request heightmap via terrain_data MCP tool with resolution=target_res
# (AI calls: terrain_data generate_heightmap lat=X lon=Y resolution=1009)
# IMPORTANT: Use the height_scale returned by preview_elevation

# Step 4: Calculate Z scale from height_scale
# ⚠️ ALWAYS calculate — do NOT guess or hardcode!
height_scale = 250  # example: from preview_elevation suggested_height_scale
z_scale = int(20000 / height_scale)  # = 80 for height_scale=250
# For mountains (height_scale=27): z_scale = 741
# For hills (height_scale=100): z_scale = 200
# For flat/gentle (height_scale=250): z_scale = 80

# Step 5: Verify downloaded heightmap dimensions
heightmap_path = "C:/path/to/downloaded_heightmap.png"
dims = unreal.LandscapeService.get_heightmap_dimensions(heightmap_path)
print(f"Downloaded: {dims.width}x{dims.height}")

# Step 5: Resize if needed (safety check)
if dims.width != target_res or dims.height != target_res:
    resize = unreal.LandscapeService.resize_heightmap(heightmap_path, target_res, target_res)
    heightmap_path = resize.output_file
    print(f"Resized to {target_res}x{target_res}")

# Step 6: Create landscape
result = unreal.LandscapeService.create_landscape(
    location=unreal.Vector(0, 0, 0),
    rotation=unreal.Rotator(0, 0, 0),
    scale=unreal.Vector(100, 100, z_scale),
    sections_per_component=sections,
    quads_per_section=quads,
    component_count_x=comp_x,
    component_count_y=comp_y,
    landscape_label="RealWorldTerrain"
)

# Step 7: Import
if result.success:
    import_result = unreal.LandscapeService.import_heightmap("RealWorldTerrain", heightmap_path)
    if import_result.success:
        print(f"Terrain imported successfully at {import_result.resolution}")
    else:
        print(f"Import failed: {import_result.error_message}")
```
