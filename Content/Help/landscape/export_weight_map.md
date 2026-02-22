# export_weight_map

Export a single paint layer's weight data as an 8-bit grayscale PNG. White (255) = full weight, black (0) = zero weight.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `landscape` | string | Yes | Name or label of the landscape |
| `layer` | string | Yes | Paint layer name (e.g. `"L1"`) |
| `output_file_path` | string | Yes | Absolute path for the output `.png` file |

## Returns

`FWeightMapExportResult` with fields:
- `success` — bool
- `file_path` — actual path written
- `width`, `height` — image dimensions in pixels (= landscape vertex resolution)
- `error_message` — description if failed

## Example

```python
import unreal

svc = unreal.LandscapeService

# Export grass layer for backup
result = svc.export_weight_map("landscape4", "L1", "C:/Temp/grass_weights.png")
print(f"Exported {result.width}x{result.height} weight map to {result.file_path}")

# Export all layers
for layer in svc.list_layers("landscape4"):
    svc.export_weight_map("landscape4", layer.layer_name,
        f"C:/Temp/{layer.layer_name}_weights.png")
```
