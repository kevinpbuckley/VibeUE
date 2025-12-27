# get_info

Get detailed information about a specific function in a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| FunctionName | string | Yes | Name of the function to inspect |

## Examples

### Get Function Info
```json
{
  "Action": "get_info",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"FunctionName\": \"CalculateDamage\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "Function": {
    "Name": "CalculateDamage",
    "Description": "Calculate final damage with modifiers",
    "Category": "Combat",
    "IsPure": false,
    "AccessSpecifier": "Public",
    "Inputs": [
      {
        "Name": "BaseDamage",
        "Type": "Float"
      },
      {
        "Name": "DamageType",
        "Type": "EDamageType"
      }
    ],
    "Outputs": [
      {
        "Name": "FinalDamage",
        "Type": "Float"
      },
      {
        "Name": "WasCritical",
        "Type": "Boolean"
      }
    ],
    "NodeCount": 12
  }
}
```

## Tips

- Use this to understand function signatures before calling them
- Shows all input and output parameters with their types
- NodeCount indicates the complexity of the function's graph
- Inherited functions will show as part of the function list
