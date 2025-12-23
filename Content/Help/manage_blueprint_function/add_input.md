# add_input

Add an input parameter to a function.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| blueprint_name | string | Yes | Name of the Blueprint (short name, not full path) |
| function_name | string | Yes | Name of the function to modify |
| param_name | string | Yes | Name for the new input parameter |
| param_type | string | Yes | Type of the parameter (e.g., "Float", "Integer", "Boolean", "String", "Vector", "LinearColor") |
| default_value | any | No | Default value for the parameter |

## Examples

### Add Float Input
```json
{
  "Action": "add_input",
  "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "TakeDamage", "param_name": "DamageAmount", "param_type": "Float"}
}
```

### Add Boolean Input
```json
{
  "Action": "add_input",
  "ParamsJson": {"blueprint_name": "BP_Enemy", "function_name": "SetAggressive", "param_name": "bAggressive", "param_type": "Boolean"}
}
```

### Add LinearColor Input
```json
{
  "Action": "add_input",
  "ParamsJson": {"blueprint_name": "BP_Widget", "function_name": "SetColors", "param_name": "InColor", "param_type": "LinearColor"}
}
```

### Add Vector Input
```json
{
  "Action": "add_input",
  "ParamsJson": {"blueprint_name": "BP_Projectile", "function_name": "Launch", "param_name": "Direction", "param_type": "Vector"}
}
```

## Returns

```json
{
  "success": true,
  "blueprint_name": "BP_Player",
  "function_name": "TakeDamage",
  "param_name": "DamageAmount",
  "param_type": "Float",
  "message": "Input parameter added successfully"
}
```

## Tips

- Common types: Float, Integer, Boolean, String, Name, Text, Vector, Rotator, Transform, LinearColor, Actor, Object
- Parameter names should use PascalCase or camelCase  
- Boolean parameters conventionally start with 'b' prefix
- Input parameters appear as pins on the function entry node
- For color parameters, use "LinearColor" as the type
