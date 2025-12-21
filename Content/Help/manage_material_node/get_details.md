# get_details

Get detailed information about a specific expression node.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_id | string | Yes | ID of the expression |

## Examples

```json
{
  "action": "get_details",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionConstant_0"
}
```

## Returns

- `success`: true/false
- `expression_id`: Node identifier
- `class_name`: Expression type
- `position`: Graph position
- `inputs`: Array of input pins
- `outputs`: Array of output pins
- `properties`: Editable properties with current values

## Tips

- More detailed than `list` - shows pins and properties
- Use to understand node capabilities before connecting
- Property values can be modified with `set_property`
