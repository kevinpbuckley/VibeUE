# connect_spline_points

Create a spline segment connecting two existing control points. The segment defines the curved path between them and carries terrain deformation and layer painting settings.

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `landscape` | string | Yes | — | Name or label of the landscape |
| `start_point_index` | int | Yes | — | Index of the start control point |
| `end_point_index` | int | Yes | — | Index of the end control point |
| `tangent_length` | float | No | `0.0` | Tangent arm length. `0.0` = auto-calculate from point distance. **Non-zero values (including negative) are used as-is.** Negative tangents reverse the mesh flow direction along the segment. |
| `paint_layer_name` | string | No | `""` | Layer painted under this segment |
| `raise_terrain` | bool | No | `True` | Raise terrain under segment |
| `lower_terrain` | bool | No | `True` | Lower terrain under segment |

## Returns

`bool` — `True` if the segment was created.

## Tangent Length Notes

- `0.0` (default) → auto-calculate as half the distance between the two points
- Positive value → tangent arm length in world units, forward direction
- **Negative value** → reverses the spline mesh flow along the segment. Use when `get_spline_info` reports negative `start_tangent_length` on a source segment (e.g. water rivers where mesh flow direction matters)

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

# Preserve exact tangent length (including negative) from a source spline
info = svc.get_spline_info("source_landscape")
for seg in info.segments:
    svc.connect_spline_points(
        "target_landscape",
        seg.start_point_index,
        seg.end_point_index,
        tangent_length=seg.start_tangent_length  # passes negative values correctly
    )
```
