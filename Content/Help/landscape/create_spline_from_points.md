# create_spline_from_points

High-level convenience: create an entire spline path from an array of world locations. Creates control points and connects them sequentially in one call. This is the "just draw me a road" action.

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `landscape` | string | Yes | — | Name or label of the landscape |
| `world_locations` | Vector[] | Yes | — | Ordered world-space positions along the path |
| `width` | float | No | `500.0` | Half-width for all control points |
| `side_falloff` | float | No | `500.0` | Side falloff for all control points |
| `end_falloff` | float | No | `500.0` | End falloff for all control points |
| `paint_layer_name` | string | No | `""` | Layer painted under the entire spline |
| `raise_terrain` | bool | No | `True` | Raise terrain along spline |
| `lower_terrain` | bool | No | `True` | Lower terrain along spline |
| `closed_loop` | bool | No | `False` | Connect last point back to first |

## Returns

`FLandscapeSplineInfo` describing all created points and segments.

## Example

```python
import unreal

svc = unreal.LandscapeService

# Create a winding dirt road
road_points = [
    unreal.Vector(-20000, 0, 500),
    unreal.Vector(-10000, 5000, 600),
    unreal.Vector(0, 0, 700),
    unreal.Vector(10000, -5000, 600),
    unreal.Vector(20000, 0, 500),
]
result = svc.create_spline_from_points(
    "landscape4", road_points,
    width=400.0, side_falloff=300.0,
    paint_layer_name="L2",
    raise_terrain=True, lower_terrain=True)

print(f"Created {result.num_control_points} points, {result.num_segments} segments")

# Apply terrain deformation
svc.apply_splines_to_landscape("landscape4")
```
