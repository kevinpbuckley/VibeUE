# get_weights_in_region

Bulk-read layer weights as a flat float array (0.0–1.0), row-major. Equivalent to `get_height_in_region` but for paint layer data.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `landscape` | string | Yes | Name or label of the landscape |
| `layer` | string | Yes | Layer to read (e.g. `"L1"`) |
| `start_x` | int | Yes | Start vertex X |
| `start_y` | int | Yes | Start vertex Y |
| `size_x` | int | Yes | Width in vertices |
| `size_y` | int | Yes | Height in vertices |

## Returns

Row-major `TArray<float>` of size `size_x * size_y`. Values are 0.0–1.0. Empty array on failure.

## Example

```python
import unreal

svc = unreal.LandscapeService
info = svc.get_landscape_info("landscape4")

# Read all grass weights
weights = svc.get_weights_in_region("landscape4", "L1",
    0, 0, info.resolution_x, info.resolution_y)

# Find the vertex with maximum grass weight
max_w = max(weights)
max_i = weights.index(max_w)
row, col = divmod(max_i, info.resolution_x)
print(f"Max grass at vertex ({col}, {row}) = {max_w:.2f}")
```
