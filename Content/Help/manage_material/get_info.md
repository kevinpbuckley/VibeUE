# get_info

Get comprehensive information about a material including properties, parameters, and node graph summary.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to material |

## Examples

```json
{
  "action": "get_info",
  "material_path": "/Game/Materials/MyMaterial"
}
```

## Returns

- `success`: true/false
- `material_path`: Path to material
- `material_name`: Name of material
- `blend_mode`: Current blend mode
- `shading_model`: Current shading model
- `two_sided`: Whether two-sided rendering is enabled
- `is_masked`: Whether using masked opacity
- `expressions_count`: Number of material expressions/nodes
- `parameters`: List of exposed parameters
- Additional material properties

## Tips

- Use this to understand a material before making changes
- For detailed property metadata, use `get_property_info`
- For just parameters, use `list_parameters`
