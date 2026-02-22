# delete_all_splines

Clear all spline control points and segments from the landscape in one operation.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `landscape` | string | Yes | Name or label of the landscape |

## Returns

`bool` — `True` if cleared successfully (also returns `True` if there were no splines).

## Example

```python
import unreal

svc = unreal.LandscapeService

# Remove all splines to start fresh
svc.delete_all_splines("landscape4")

# Verify
info = svc.get_spline_info("landscape4")
print(f"Points remaining: {info.num_control_points}")  # → 0
```
