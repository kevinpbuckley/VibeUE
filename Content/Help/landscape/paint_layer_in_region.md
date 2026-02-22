# paint_layer_in_region

Paint a layer across a rectangular vertex-index region with uniform weight. Much faster than calling `paint_layer_at_location` in a loop — a single bulk write covers the whole region.

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `landscape` | string | Yes | — | Name or label of the landscape |
| `layer` | string | Yes | — | Paint layer name (e.g. `"L1"`) |
| `start_x` | int | Yes | — | First vertex X index (inclusive) |
| `start_y` | int | Yes | — | First vertex Y index (inclusive) |
| `size_x` | int | Yes | — | Region width in vertices |
| `size_y` | int | Yes | — | Region height in vertices |
| `strength` | float | No | `1.0` | Target weight (0.0–1.0) |

## Example

```python
import unreal

svc = unreal.LandscapeService
info = svc.get_landscape_info("landscape4")

# Paint the entire landscape with full grass weight
svc.paint_layer_in_region("landscape4", "L1",
    0, 0, info.resolution_x, info.resolution_y,
    strength=1.0)

# Paint just the southern half with dirt at 80% strength
half_y = info.resolution_y // 2
svc.paint_layer_in_region("landscape4", "L2",
    0, half_y, info.resolution_x, half_y,
    strength=0.8)
```
