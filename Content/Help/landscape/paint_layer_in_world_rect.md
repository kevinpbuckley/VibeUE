# paint_layer_in_world_rect

Paint a layer across a world-space rectangular region with uniform weight. Converts world coordinates to vertex indices and delegates to `paint_layer_in_region`.

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `landscape` | string | Yes | — | Name or label of the landscape |
| `layer` | string | Yes | — | Paint layer name (e.g. `"L1"`) |
| `world_min_x` | float | Yes | — | Minimum X world coordinate |
| `world_min_y` | float | Yes | — | Minimum Y world coordinate |
| `world_max_x` | float | Yes | — | Maximum X world coordinate |
| `world_max_y` | float | Yes | — | Maximum Y world coordinate |
| `strength` | float | No | `1.0` | Target weight (0.0–1.0) |

## Example

```python
import unreal

svc = unreal.LandscapeService

# Paint a 20km×20km region with rock layer
svc.paint_layer_in_world_rect("landscape4", "L3",
    -10000.0, -10000.0, 10000.0, 10000.0,
    strength=0.8)

# Paint a 5km road corridor with dirt
svc.paint_layer_in_world_rect("landscape4", "L2",
    -2500.0, -50000.0, 2500.0, 50000.0,
    strength=1.0)
```
