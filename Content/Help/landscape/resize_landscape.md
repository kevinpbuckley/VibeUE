# resize_landscape

Resize a landscape to a new component grid, bilinearly resampling height and weight data to match the new resolution.

> **Warning:** This is a destructive operation. The original landscape is deleted and replaced by a new one. Foliage instances and splines are NOT transferred — rebuild them after resizing.

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `landscape` | string | Yes | — | Name or label of the landscape to resize |
| `new_component_count_x` | int | Yes | — | New number of components in X |
| `new_component_count_y` | int | Yes | — | New number of components in Y |
| `new_quads_per_section` | int | No | `-1` | Quads per section (-1 = keep current) |
| `new_sections_per_component` | int | No | `-1` | Sections per component (-1 = keep current) |

## Returns

`FLandscapeCreateResult` for the new landscape:
- `success` — bool
- `actor_label` — label of the new landscape
- `error_message`

## Resolution Formula

`resolution = (component_count * quads_per_section * sections_per_component) + 1`

Common configurations:
| Components | Quads | Sections | Resolution |
|-----------|-------|----------|-----------|
| 8×8 | 63 | 1 | 505×505 |
| 8×8 | 127 | 1 | 1017×1017 |
| 16×16 | 63 | 1 | 1009×1009 |

## Example

```python
import unreal

svc = unreal.LandscapeService

# Check current resolution
info = svc.get_landscape_info("landscape4")
print(f"Current resolution: {info.resolution_x}x{info.resolution_y}")

# Double the component count (8x8 → 16x16), keeping quads/section
result = svc.resize_landscape("landscape4",
    new_component_count_x=16,
    new_component_count_y=16)

if result.success:
    new_info = svc.get_landscape_info(result.actor_label)
    print(f"New resolution: {new_info.resolution_x}x{new_info.resolution_y}")
```
