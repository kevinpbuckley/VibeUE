# create_spline_point

Create a new landscape spline control point. Control points are the waypoints that define a road, river, or path. Connect them with `connect_spline_points`, then call `apply_splines_to_landscape` to deform the terrain.

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `landscape` | string | Yes | — | Name or label of the landscape |
| `world_location` | Vector | Yes | — | World-space position for the point |
| `width` | float | No | `500.0` | Half-width of spline influence (world units) |
| `side_falloff` | float | No | `500.0` | Falloff at the sides (world units) |
| `end_falloff` | float | No | `500.0` | Falloff at start/end tips (world units) |
| `paint_layer_name` | string | No | `""` | Layer painted under the spline (empty = none) |
| `raise_terrain` | bool | No | `True` | Whether to raise terrain to spline level |
| `lower_terrain` | bool | No | `True` | Whether to lower terrain to spline level |

## Returns

`FSplineCreateResult` with fields:
- `success` — bool
- `point_index` — index of the new control point
- `error_message`

## Example

```python
import unreal

svc = unreal.LandscapeService

# Create two road waypoints
r0 = svc.create_spline_point("landscape4", unreal.Vector(-10000, 0, 500),
    width=400.0, paint_layer_name="L2")
r1 = svc.create_spline_point("landscape4", unreal.Vector(10000, 0, 500),
    width=400.0, paint_layer_name="L2")

print(f"Created points {r0.point_index} and {r1.point_index}")
```
