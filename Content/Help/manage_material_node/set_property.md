# set_property

Set a property value on an expression node.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_id | string | Yes | ID of the expression |
| property_name | string | Yes | Name of the property to set |
| value | string/number/bool | Yes | New value for the property |

## Examples

Set constant value:
```json
{
  "action": "set_property",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionConstant_0",
  "property_name": "R",
  "value": 0.5
}
```

Set vector constant:
```json
{
  "action": "set_property",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionConstant3Vector_0",
  "property_name": "Constant",
  "value": "(R=1.0,G=0.5,B=0.0)"
}
```

## Returns

- `success`: true/false
- `property_name`: Property that was set
- `value`: New value
- `message`: Confirmation message

## Tips

- Property names are case-sensitive
- For constants: use "R" for scalar, "Constant" for vector
- Remember to compile the material after changing values
- Use `list_properties` to see available properties
