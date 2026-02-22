# set_hole_at_location

Punch or fill a circular hole in the landscape at a world position. Holes are used for caves, tunnels, and anywhere the terrain surface should be absent.

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `landscape` | string | Yes | — | Name or label of the landscape |
| `world_x` | float | Yes | — | Center X in world units |
| `world_y` | float | Yes | — | Center Y in world units |
| `brush_radius` | float | Yes | — | Hole radius in world units |
| `create_hole` | bool | No | `True` | `True` to punch a hole, `False` to fill |

## Notes

Internally writes to the landscape visibility layer (weight 255 = hole, 0 = solid). The terrain mesh disappears at hole vertices, allowing caves or tunnels to be visible underneath.

## Example

```python
import unreal

svc = unreal.LandscapeService

# Punch a cave entrance
svc.set_hole_at_location("landscape4", 5000.0, 3000.0, 500.0, create_hole=True)

# Fill it back
svc.set_hole_at_location("landscape4", 5000.0, 3000.0, 500.0, create_hole=False)

# Create a series of cave openings along a cliff
for i in range(5):
    svc.set_hole_at_location("landscape4", -20000.0 + i * 3000.0, 10000.0, 400.0, create_hole=True)
```
