# connect_spline_points

Create a spline segment connecting two existing control points. The segment defines the curved path between them and carries terrain deformation and layer painting settings.

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `landscape` | string | Yes | — | Name or label of the landscape |
| `start_point_index` | int | Yes | — | Index of the start control point |
| `end_point_index` | int | Yes | — | Index of the end control point |
| `tangent_length` | float | No | `0.0` | Tangent arm length (0 = auto from distance) |
| `paint_layer_name` | string | No | `""` | Layer painted under this segment |
| `raise_terrain` | bool | No | `True` | Raise terrain under segment |
| `lower_terrain` | bool | No | `True` | Lower terrain under segment |

## Returns

`bool` — `True` if the segment was created.

## Example

```python
import unreal

svc = unreal.LandscapeService

# Create two waypoints then connect them
r0 = svc.create_spline_point("landscape4", unreal.Vector(-10000, 0, 500))
r1 = svc.create_spline_point("landscape4", unreal.Vector(0, 5000, 600))
r2 = svc.create_spline_point("landscape4", unreal.Vector(10000, 0, 500))

svc.connect_spline_points("landscape4", r0.point_index, r1.point_index, paint_layer_name="L2")
svc.connect_spline_points("landscape4", r1.point_index, r2.point_index, paint_layer_name="L2")

svc.apply_splines_to_landscape("landscape4")
```
