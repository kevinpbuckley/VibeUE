# add_output

Add an output parameter (return value) to a function.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| FunctionName | string | Yes | Name of the function to modify |
| ParamName | string | Yes | Name for the new output parameter |
| ParamType | string | Yes | Type of the parameter (e.g., "Float", "Integer", "Boolean", "String", "Vector") |

## Examples

### Add Float Output
```json
{
  "Action": "add_output",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"FunctionName\": \"GetHealth\", \"ParamName\": \"CurrentHealth\", \"ParamType\": \"Float\"}"
}
```

### Add Boolean Output
```json
{
  "Action": "add_output",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Enemy\", \"FunctionName\": \"IsAlive\", \"ParamName\": \"bAlive\", \"ParamType\": \"Boolean\"}"
}
```

### Add Object Output
```json
{
  "Action": "add_output",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Spawner\", \"FunctionName\": \"SpawnEnemy\", \"ParamName\": \"SpawnedActor\", \"ParamType\": \"Actor\"}"
}
```

### Add Multiple Outputs (call multiple times)
```json
{
  "Action": "add_output",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Inventory\", \"FunctionName\": \"GetItemInfo\", \"ParamName\": \"ItemName\", \"ParamType\": \"String\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "FunctionName": "GetHealth",
  "ParamName": "CurrentHealth",
  "ParamType": "Float",
  "Message": "Output parameter added successfully"
}
```

## Tips

- Functions can have multiple output parameters (call this action multiple times)
- Output parameters appear as pins on the Return Node
- The first output is typically the "main" return value
- For simple returns, name the output "ReturnValue"
- Pure functions commonly return calculated values
- Compile after adding parameters to update the function signature
