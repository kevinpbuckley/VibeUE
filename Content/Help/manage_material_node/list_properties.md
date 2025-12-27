# list_properties

List all editable properties for an expression node.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_id | string | Yes | ID of the expression |

## Examples

```json
{
  "action": "list_properties",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionConstant_0"
}
```

## Returns

- `success`: true/false
- `expression_id`: Node identifier
- `properties`: Array of property info objects
- `total_count`: Number of properties

Each property includes:
- `name`: Property name for get/set
- `type`: Property type
- `value`: Current value
- `editable`: Whether it can be modified

## Tips

- Use this before get_property/set_property to find valid names
- Not all properties are editable
- Common editable properties: R (scalar constant), Constant (vector), ParameterName
