# get_property

Get a property value from an expression node.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_id | string | Yes | ID of the expression |
| property_name | string | Yes | Name of the property to get |

## Examples

Get constant value:
```json
{
  "action": "get_property",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionConstant_0",
  "property_name": "R"
}
```

Get parameter name:
```json
{
  "action": "get_property",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionScalarParameter_0",
  "property_name": "ParameterName"
}
```

## Returns

- `success`: true/false
- `property_name`: Name of property
- `value`: Current value
- `property_type`: Type of the property

## Tips

- Use `list_properties` to see available property names
- Property names are case-sensitive
- Common properties: R (for constants), ParameterName, DefaultValue
