# import_weight_map

Import layer weights from an 8-bit grayscale PNG. The R channel is used (for grayscale images, R=G=B). Image dimensions must match the landscape vertex resolution exactly.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `landscape` | string | Yes | Name or label of the landscape |
| `layer` | string | Yes | Target paint layer name (e.g. `"L3"`) |
| `file_path` | string | Yes | Absolute path to source PNG file |

## Returns

`FWeightMapImportResult` with fields:
- `success` — bool
- `vertices_modified` — number of vertices written
- `error_message` — description if failed

## Notes

- Use `get_landscape_info` to find the required image dimensions (`resolution_x` × `resolution_y`)
- For fastest results, pre-process weight maps externally and import in bulk rather than painting stroke by stroke

## Example

```python
import unreal

svc = unreal.LandscapeService

# Check required image size
info = svc.get_landscape_info("landscape4")
print(f"Need PNG: {info.resolution_x}x{info.resolution_y}")

# Import a pre-painted rock mask
result = svc.import_weight_map("landscape4", "L3", "C:/Temp/rock_mask.png")
print(f"Imported {result.vertices_modified} vertices")
```
