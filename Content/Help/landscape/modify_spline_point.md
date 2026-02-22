# modify_spline_point

Modify an existing spline control point's position, properties, and rotation. Pass `-1.0` for numeric fields to leave them unchanged. Pass `"__unchanged__"` for `paint_layer_name` to leave it unchanged.

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `landscape` | string | Yes | — | Name or label of the landscape |
| `point_index` | int | Yes | — | Index of the control point to modify |
| `world_location` | Vector | Yes | — | New world-space location |
| `width` | float | No | `-1.0` | New half-width (-1 = unchanged) |
| `side_falloff` | float | No | `-1.0` | New side falloff (-1 = unchanged) |
| `end_falloff` | float | No | `-1.0` | New end falloff (-1 = unchanged) |
| `paint_layer_name` | string | No | `"__unchanged__"` | New paint layer (`"__unchanged__"` = no change) |
| `rotation` | Rotator | No | `Rotator(0,0,0)` | Explicit rotation to apply (only used if `auto_calc_rotation=False`) |
| `auto_calc_rotation` | bool | No | `True` | If `True` (default), rotation is recomputed from connected segments. Set `False` to apply `rotation` directly. |

## Returns

`bool` — `True` if modified successfully.

## Example

```python
import unreal

svc = unreal.LandscapeService

info = svc.get_spline_info("landscape4")
if info.success and info.num_control_points > 0:
    # Move the first point higher (auto-calc rotation from segments)
    pt = info.control_points[0]
    new_loc = pt.location + unreal.Vector(0, 0, 500)
    svc.modify_spline_point("landscape4", 0, new_loc, width=600.0)

    # Set an explicit rotation (e.g. to match a source landscape's control point)
    svc.modify_spline_point(
        "landscape4", 0, pt.location,
        rotation=unreal.Rotator(pt.rotation.pitch, pt.rotation.yaw, pt.rotation.roll),
        auto_calc_rotation=False
    )
```

## Notes

- When `auto_calc_rotation=True` (default), calls UE's `AutoCalcRotation()` which derives orientation from the connected segment directions. This is almost always what you want for natural-looking splines.
- Use `auto_calc_rotation=False` only when you need to precisely reproduce the rotation from a `get_spline_info` result (e.g. matching `pt.rotation` exactly).
