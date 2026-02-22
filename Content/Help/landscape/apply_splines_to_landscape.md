# apply_splines_to_landscape

Apply terrain deformation and layer painting for all splines on the landscape. This triggers UE's built-in spline → landscape rasterization.

Call this after creating/modifying splines to actually push the height changes and paint into the landscape data.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `landscape` | string | Yes | Name or label of the landscape |

## Returns

`bool` — `True` if applied successfully.

## Notes

- This is a relatively slow operation as it rasterizes all spline segments.
- Call it once at the end of all spline creation, not after each individual spline.

## Example

```python
import unreal

svc = unreal.LandscapeService

# Create an entire road network
road_a = [unreal.Vector(-20000, 0, 500), unreal.Vector(20000, 0, 500)]
road_b = [unreal.Vector(0, -20000, 500), unreal.Vector(0, 20000, 500)]

svc.create_spline_from_points("landscape4", road_a,
    width=400.0, paint_layer_name="L2")
svc.create_spline_from_points("landscape4", road_b,
    width=400.0, paint_layer_name="L2")

# Apply all splines at once
svc.apply_splines_to_landscape("landscape4")
```
