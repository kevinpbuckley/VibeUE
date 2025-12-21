# add_input

Add an input parameter to a function.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| FunctionName | string | Yes | Name of the function to modify |
| ParamName | string | Yes | Name for the new input parameter |
| ParamType | string | Yes | Type of the parameter (e.g., "Float", "Integer", "Boolean", "String", "Vector") |
| DefaultValue | any | No | Default value for the parameter |

## Examples

### Add Float Input
```json
{
  "Action": "add_input",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"FunctionName\": \"TakeDamage\", \"ParamName\": \"DamageAmount\", \"ParamType\": \"Float\", \"DefaultValue\": 0}"
}
```

### Add Boolean Input
```json
{
  "Action": "add_input",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Enemy\", \"FunctionName\": \"SetAggressive\", \"ParamName\": \"bAggressive\", \"ParamType\": \"Boolean\", \"DefaultValue\": true}"
}
```

### Add Object Input
```json
{
  "Action": "add_input",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Weapon\", \"FunctionName\": \"AttackTarget\", \"ParamName\": \"Target\", \"ParamType\": \"Actor\"}"
}
```

### Add Vector Input
```json
{
  "Action": "add_input",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Projectile\", \"FunctionName\": \"Launch\", \"ParamName\": \"Direction\", \"ParamType\": \"Vector\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "FunctionName": "TakeDamage",
  "ParamName": "DamageAmount",
  "ParamType": "Float",
  "Message": "Input parameter added successfully"
}
```

## Tips

- Common types: Float, Integer, Boolean, String, Name, Text, Vector, Rotator, Transform, Actor, Object
- Parameter names should use PascalCase or camelCase
- Boolean parameters conventionally start with 'b' prefix
- Input parameters appear as pins on the function entry node
- Compile after adding parameters to update the function signature
