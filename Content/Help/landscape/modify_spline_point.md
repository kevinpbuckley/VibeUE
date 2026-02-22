# modify_spline_point

Modify an existing spline control point's position and properties. Pass `-1.0` for numeric fields to leave them unchanged. Pass `"__unchanged__"` for `paint_layer_name` to leave it unchanged.

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

## Returns

`bool` — `True` if modified successfully.

## Example

```python
import unreal

svc = unreal.LandscapeService

info = svc.get_spline_info("landscape4")
if info.success and info.num_control_points > 0:
    # Move the first point higher
    pt = info.control_points[0]
    new_loc = pt.location + unreal.Vector(0, 0, 500)
    svc.modify_spline_point("landscape4", 0, new_loc, width=600.0)
```
