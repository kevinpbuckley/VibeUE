---
name: intelligent-sculpting
description: v3 semantic terrain features (create_mountain/valley/ridge/plateau/crater/terraces, apply_erosion, blend_terrain_features), terrain analysis (analyze_terrain, get_slope/normal_at_location, get_slope_map, find_flat_areas), mesh projection (project_mesh_to_landscape, sample_mesh_heights), batch line traces, and a complete v3 workflow example
---

# v3 — Intelligent Sculpting

### Semantic Terrain Features

Use these to build natural-looking terrain shapes procedurally.

#### Create a Mountain

```python
import unreal

# Raise terrain in a smooth radial peak. Height is a world-unit delta.
# Sharpness=1.0 = cosine bell, 2.0+ = sharper summit
unreal.LandscapeService.create_mountain(
    "MyTerrain",
    center_x=0.0, center_y=0.0,
    radius=5000.0,     # base radius (world units)
    height=3000.0,     # peak height delta
    sharpness=1.5,
    b_add_noise=True,
    seed=42
)
```

#### Create a Valley

```python
import unreal

unreal.LandscapeService.create_valley(
    "MyTerrain",
    center_x=5000.0, center_y=0.0,
    radius=4000.0,
    depth=2000.0,      # how far to lower terrain
    sharpness=1.0,
    b_add_noise=True,
    seed=7
)
```

#### Create a Ridge

```python
import unreal

# Elongated raised spine from start to end point, with falloff perpendicular to spine
unreal.LandscapeService.create_ridge(
    "MyTerrain",
    start_x=-8000.0, start_y=0.0,
    end_x=8000.0,    end_y=2000.0,
    width=2000.0,    # half-width perpendicular to spine
    height=2500.0,
    sharpness=1.2,
    b_add_noise=True,
    seed=13
)
```

#### Create a Plateau

```python
import unreal

# Flat-topped elevated area with smooth edges
unreal.LandscapeService.create_plateau(
    "MyTerrain",
    center_x=0.0, center_y=0.0,
    radius=3000.0,       # flat-top radius
    height=2000.0,
    edge_blend=800.0     # transition distance from flat to ground
)
```

#### Create a Crater

```python
import unreal

# Bowl-shaped depression with optional raised rim
unreal.LandscapeService.create_crater(
    "MyTerrain",
    center_x=2000.0, center_y=2000.0,
    radius=1500.0,
    depth=800.0,
    rim_height=200.0    # set 0.0 for no rim
)
```

#### Create Terraces

```python
import unreal

# Quantize height into horizontal stepped levels
unreal.LandscapeService.create_terraces(
    "MyTerrain",
    center_x=0.0, center_y=0.0,
    radius=6000.0,
    num_terraces=6,
    smoothness=0.3    # 0=sharp steps, 1=fully smoothed
)
```

#### Apply Erosion

```python
import unreal

# Thermal erosion simulation - moves material from steep to lower neighbors
# More iterations = stronger effect. Keep radius focused to avoid long runtimes.
unreal.LandscapeService.apply_erosion(
    "MyTerrain",
    center_x=0.0, center_y=0.0,
    radius=5000.0,
    iterations=500,    # each 100 iterations = 1 full pass
    strength=1.0,
    seed=0
)
```

#### Blend / Smooth a Region

```python
import unreal

# 3×3 box average smooth across a region - use to blend between features
unreal.LandscapeService.blend_terrain_features(
    "MyTerrain",
    center_x=0.0, center_y=0.0,
    radius=3000.0,
    blend_weight=0.8    # 0=no change, 1=fully averaged
)
```

---

### Terrain Analysis

Query slope, normal, and statistics without modifying the terrain.

#### Analyze a Region

```python
import unreal

# Returns min/max/avg height, avg slope, max slope, roughness
analysis = unreal.LandscapeService.analyze_terrain(
    "MyTerrain",
    center_x=0.0, center_y=0.0,
    radius=5000.0    # 0 = whole landscape
)
print(f"Height: {analysis.min_height:.0f} to {analysis.max_height:.0f}")
print(f"Avg slope: {analysis.average_slope_degrees:.1f}°, Max: {analysis.max_slope_degrees:.1f}°")
print(f"Roughness: {analysis.roughness:.1f}")
```

#### Get Slope / Normal at a Point

```python
import unreal

slope = unreal.LandscapeService.get_slope_at_location("MyTerrain", 1000.0, 2000.0)
print(f"Slope: {slope:.1f}°")   # 0=flat, 90=vertical

normal = unreal.LandscapeService.get_normal_at_location("MyTerrain", 1000.0, 2000.0)
print(f"Normal: {normal}")
```

#### Get a Slope Map

```python
import unreal

# Returns per-vertex slope in degrees, same row-major layout as get_height_in_region
# Pass all zeros to get the whole landscape
slopes = unreal.LandscapeService.get_slope_map(
    "MyTerrain",
    min_world_x=-5000.0, min_world_y=-5000.0,
    max_world_x=5000.0,  max_world_y=5000.0
)
max_slope = max(slopes)
print(f"Max slope in region: {max_slope:.1f}°")
```

#### Find Flat Areas

```python
import unreal

# Returns world-space centers of clusters where slope < threshold
flat_spots = unreal.LandscapeService.find_flat_areas(
    "MyTerrain",
    max_slope_degrees=5.0,    # anything below this is "flat"
    min_radius=500.0,         # ignore tiny flat patches
    max_results=5
)
for spot in flat_spots:
    print(f"Flat area at: {spot}")
```

---

### Mesh Projection

Conform the landscape heightmap to match a static mesh actor's surface.

#### Project a Single Mesh

```python
import unreal

# The mesh actor must exist in the scene. Use its actor label (visible in Outliner).
# blend_weight=1.0 fully replaces landscape height with mesh surface height.
result = unreal.LandscapeService.project_mesh_to_landscape(
    "MyTerrain",
    mesh_actor_label="SM_CliffFace",
    blend_weight=1.0,
    b_additive=False
)
print(f"Modified {result.vertices_modified} vertices")
```

#### Project Multiple Meshes

There is no batch projector — call `project_mesh_to_landscape` once per mesh actor:

```python
import unreal

for label in ["SM_Rock01", "SM_Rock02", "SM_Cliff"]:
    r = unreal.LandscapeService.project_mesh_to_landscape(
        "MyTerrain",
        mesh_actor_label=label,
        blend_weight=0.9,
        b_additive=False
    )
    print(f"{'OK' if r.success else 'FAIL'} ({label}): {r.vertices_modified} verts")
```

#### Sample Mesh Heights (preview without modifying)

```python
import unreal

# Get hit locations from downward traces over the mesh, without changing terrain
hits = unreal.LandscapeService.sample_mesh_heights(
    "SM_CliffFace",
    center_x=1000.0, center_y=2000.0,
    radius=500.0,
    sample_count=10    # 10×10 grid = 100 traces
)
zs = [h.z for h in hits]
print(f"Mesh height range: {min(zs):.0f} to {max(zs):.0f}")
```

---

### Batch Line Traces

Perform many line traces in a single call for efficient world sampling.

#### Arbitrary Batch Traces

```python
import unreal

starts = [unreal.Vector(x * 200, 0, 10000) for x in range(20)]
ends   = [unreal.Vector(x * 200, 0, -1000) for x in range(20)]

hits = unreal.LandscapeService.batch_line_trace(starts, ends)
for hit in hits:
    if hit.b_hit:
        print(f"Hit {hit.actor_name} at Z={hit.hit_location.z:.0f}, normal={hit.hit_normal}")
```

#### Grid of Downward Traces

```python
import unreal

# Traces a GridResolution×GridResolution grid of downward rays over a region
hits = unreal.LandscapeService.batch_line_trace_grid(
    origin_x=-5000.0, origin_y=-5000.0,
    width=10000.0,    height=10000.0,
    grid_resolution=20,
    start_z=50000.0,
    end_z=-5000.0
)
terrain_hits = [h for h in hits if h.b_hit]
print(f"{len(terrain_hits)}/{len(hits)} hits")
zs = [h.hit_location.z for h in terrain_hits]
if zs: print(f"Height range: {min(zs):.0f} to {max(zs):.0f}")
```

---

### Complete v3 Workflow Example

Build a natural mountain-valley scene from scratch:

```python
import unreal

ls = "MyTerrain"

# 1. Base mountain
unreal.LandscapeService.create_mountain(ls, 0, 0, 6000, 4000, sharpness=1.2, b_add_noise=True, seed=1)

# 2. Valley to the east
unreal.LandscapeService.create_valley(ls, 8000, 0, 4000, 1500, sharpness=1.0, b_add_noise=True, seed=2)

# 3. Ridge connecting them
unreal.LandscapeService.create_ridge(ls, -2000, 3000, 6000, 3000, width=1500, height=1800, sharpness=1.5, seed=3)

# 4. Erode the mountain for realism
unreal.LandscapeService.apply_erosion(ls, 0, 0, 5000, iterations=300, strength=0.8, seed=42)

# 5. Smooth transitions
unreal.LandscapeService.blend_terrain_features(ls, 0, 0, 8000, blend_weight=0.3)

# 6. Check results
analysis = unreal.LandscapeService.analyze_terrain(ls, 0, 0, 10000)
print(f"Height range: {analysis.min_height:.0f} to {analysis.max_height:.0f}")
print(f"Max slope: {analysis.max_slope_degrees:.1f}°")

# 7. Find good building sites
flat = unreal.LandscapeService.find_flat_areas(ls, max_slope_degrees=3.0, min_radius=300, max_results=3)
for spot in flat:
    print(f"Flat site: {spot}")
```
