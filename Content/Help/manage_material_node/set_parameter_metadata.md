# set_parameter_metadata

Set metadata on a parameter expression (group, priority, etc.).

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| expression_id | string | Yes | ID of the parameter expression |
| group_name | string | No | Parameter group for organization |
| sort_priority | int | No | Sort priority within group (lower = higher in list) |

## Examples

Set parameter group:
```json
{
  "action": "set_parameter_metadata",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionScalarParameter_0",
  "group_name": "Surface Properties"
}
```

Set sort priority:
```json
{
  "action": "set_parameter_metadata",
  "material_path": "/Game/Materials/M_Test",
  "expression_id": "MaterialExpressionScalarParameter_0",
  "group_name": "Surface Properties",
  "sort_priority": 0
}
```

## Returns

- `success`: true/false
- `expression_id`: Parameter expression ID
- `group_name`: Updated group name
- `sort_priority`: Updated sort priority
- `message`: Confirmation message

## Tips

- Groups organize parameters in the Material Instance editor
- Lower sort_priority values appear higher in the list
- Use consistent group names across parameters for organization
