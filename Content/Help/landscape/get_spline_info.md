# get_spline_info

Get all control points and segments on a landscape's spline component.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `landscape` | string | Yes | Name or label of the landscape |

## Returns

`FLandscapeSplineInfo` with fields:
- `success` — bool
- `num_control_points` — total control points
- `num_segments` — total segments
- `control_points` — array of `FLandscapeSplinePointInfo` (index, location, rotation, width, falloffs, layer, raise/lower flags)
- `segments` — array of `FLandscapeSplineSegmentInfo` (index, start/end point indices, tangent lengths, layer, raise/lower flags)

## Example

```python
import unreal

svc = unreal.LandscapeService

info = svc.get_spline_info("landscape4")
print(f"Spline has {info.num_control_points} points, {info.num_segments} segments")

for pt in info.control_points:
    print(f"  Point {pt.point_index}: {pt.location}, width={pt.width}")

for seg in info.segments:
    print(f"  Segment {seg.segment_index}: {seg.start_point_index} → {seg.end_point_index}")
```
