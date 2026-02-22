# get_hole_at_location

Check whether the landscape vertex at a world position is a hole (invisible) or solid.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `landscape` | string | Yes | Name or label of the landscape |
| `world_x` | float | Yes | World X coordinate |
| `world_y` | float | Yes | World Y coordinate |

## Returns

`bool` — `True` if the nearest vertex is a hole, `False` if solid.

## Example

```python
import unreal

svc = unreal.LandscapeService

is_hole = svc.get_hole_at_location("landscape4", 5000.0, 3000.0)
if is_hole:
    print("That location is a hole — cave entrance!")
else:
    print("Solid terrain here")
```
