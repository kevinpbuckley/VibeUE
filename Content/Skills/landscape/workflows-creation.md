---
name: workflows-creation
description: Workflows for creating landscapes, querying landscape info, importing/exporting heightmaps (with resolution matching), recreating splines across landscapes, copying terrain heights and paint layers between landscapes, calculating side-by-side offsets, and reading height data in a region
---

# Workflows — Creation, Import, and Copy

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

### Import Heightmap (with Resolution Matching)

**Recommended: Request heightmap at exact landscape resolution**
```python
import unreal

# 1. Decide landscape config
comp_x, comp_y = 8, 8
quads = 63
sections = 2

# 2. Calculate required resolution
res_info = unreal.LandscapeService.calculate_landscape_resolution(comp_x, comp_y, quads, sections)
print(f"Need {res_info.resolution_x}x{res_info.resolution_y} heightmap")
# → 1009×1009

# 3. Use terrain_data MCP tool with resolution=1009 to download the heightmap
# (The AI calls terrain_data generate_heightmap with resolution=1009)

# 4. Create landscape
result = unreal.LandscapeService.create_landscape(
    location=unreal.Vector(0, 0, 0),
    rotation=unreal.Rotator(0, 0, 0),
    scale=unreal.Vector(100, 100, 200),  # Z=200 for mountainous terrain
    sections_per_component=sections,
    quads_per_section=quads,
    component_count_x=comp_x,
    component_count_y=comp_y,
    landscape_label="MtFuji"
)

# 5. Import — sizes match perfectly
import_result = unreal.LandscapeService.import_heightmap("MtFuji", "C:/Heightmaps/mt_fuji_1009x1009.png")
if import_result.success:
    print(f"Imported at {import_result.resolution}")
else:
    print(f"Import failed: {import_result.error_message}")
```

**Fallback: Resize existing heightmap to match landscape**
```python
import unreal

# If you already have a heightmap at the wrong resolution
dims = unreal.LandscapeService.get_heightmap_dimensions("C:/Heightmaps/terrain_1081x1081.png")
print(f"Source: {dims.width}x{dims.height}")  # 1081×1081

# Get the landscape's actual resolution
info = unreal.LandscapeService.get_landscape_info("MyTerrain")
target_w = info.resolution_x  # e.g. 1009
target_h = info.resolution_y

# Resize the heightmap (bilinear interpolation, preserves terrain shape)
resize_result = unreal.LandscapeService.resize_heightmap(
    "C:/Heightmaps/terrain_1081x1081.png",
    target_w, target_h
    # OutputPath auto-generated as terrain_1081x1081_1009x1009.png
)
if resize_result.success:
    print(f"Resized to {resize_result.new_dimensions}")
    print(f"Output: {resize_result.output_file}")
    # Now import the resized file
    unreal.LandscapeService.import_heightmap("MyTerrain", resize_result.output_file)
```

**Alternative: Find a landscape config that matches your heightmap**
```python
import unreal

# If you want to create a landscape that matches an existing heightmap
dims = unreal.LandscapeService.get_heightmap_dimensions("C:/Heightmaps/terrain_505x505.png")
config = unreal.LandscapeService.find_landscape_config_for_resolution(dims.width, dims.height)
if config.total_components > 0:
    print(f"Config: {config.description}")
    # e.g. "8x8 components, 63 quads/section, 1 section/component → 505x505"
    # Create landscape using this config, then import directly
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
