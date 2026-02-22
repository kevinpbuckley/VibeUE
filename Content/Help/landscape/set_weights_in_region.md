# set_weights_in_region

Bulk-write layer weights from a flat float array (0.0–1.0), row-major. Values are mapped to uint8 0–255 internally.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `landscape` | string | Yes | Name or label of the landscape |
| `layer` | string | Yes | Target paint layer (e.g. `"L1"`) |
| `start_x` | int | Yes | Start vertex X |
| `start_y` | int | Yes | Start vertex Y |
| `size_x` | int | Yes | Width in vertices |
| `size_y` | int | Yes | Height in vertices |
| `weights` | float[] | Yes | Values 0.0–1.0, length must equal `size_x * size_y` |

## Example

```python
import unreal, math

svc = unreal.LandscapeService
info = svc.get_landscape_info("landscape4")

W, H = info.resolution_x, info.resolution_y

# Procedural gradient: full grass in south, zero in north
cx, cy = W / 2.0, H / 2.0
weights = []
for y in range(H):
    for x in range(W):
        # Distance from center as fraction of radius
        dist = math.sqrt((x - cx)**2 + (y - cy)**2) / (min(W, H) / 2.0)
        weights.append(max(0.0, 1.0 - dist))

svc.set_weights_in_region("landscape4", "L1", 0, 0, W, H, weights)
```
