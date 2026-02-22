# set_hole_in_region

Set hole visibility for a rectangular region of vertices. Faster than calling `set_hole_at_location` repeatedly for large areas.

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `landscape` | string | Yes | — | Name or label of the landscape |
| `start_x` | int | Yes | — | Start vertex X |
| `start_y` | int | Yes | — | Start vertex Y |
| `size_x` | int | Yes | — | Width in vertices |
| `size_y` | int | Yes | — | Height in vertices |
| `create_hole` | bool | No | `True` | `True` to punch holes, `False` to fill |

## Example

```python
import unreal

svc = unreal.LandscapeService
info = svc.get_landscape_info("landscape4")

# Hollow out a large underground chamber in the center
cx, cy = info.resolution_x // 2, info.resolution_y // 2
svc.set_hole_in_region("landscape4", cx - 50, cy - 50, 100, 100, create_hole=True)

# Fill the entire landscape (remove all holes)
svc.set_hole_in_region("landscape4", 0, 0, info.resolution_x, info.resolution_y, create_hole=False)
```
