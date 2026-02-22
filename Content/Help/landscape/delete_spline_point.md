# delete_spline_point

Remove a spline control point and all its connected segments from the landscape.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `landscape` | string | Yes | Name or label of the landscape |
| `point_index` | int | Yes | Index of the control point to remove |

## Returns

`bool` — `True` if deleted successfully.

## Notes

- Indices of higher-numbered control points shift down by 1 after deletion.
- Use `get_spline_info` to inspect the current index layout before deleting.

## Example

```python
import unreal

svc = unreal.LandscapeService

info = svc.get_spline_info("landscape4")
print(f"Before: {info.num_control_points} points")

# Delete the last control point
if info.num_control_points > 0:
    last_idx = info.num_control_points - 1
    svc.delete_spline_point("landscape4", last_idx)

info = svc.get_spline_info("landscape4")
print(f"After: {info.num_control_points} points")
```
