# get_output_connections

Get current connections to material outputs.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |

## Examples

```json
{
  "action": "get_output_connections",
  "material_path": "/Game/Materials/M_Test"
}
```

## Returns

- `success`: true/false
- `output_connections`: Array of current output connections
- `total_connected`: Number of connected outputs

Each connection includes:
- `output_property`: Material output name
- `expression_id`: Connected expression ID
- `output_name`: Which expression output pin is connected

## Tips

- Shows what expressions are driving each material output
- Use to understand current material wiring before modifications
- Useful for debugging why material looks a certain way
