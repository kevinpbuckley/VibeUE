# create

Create a new function in a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| FunctionName | string | Yes | Name for the new function |
| Description | string | No | Description/tooltip for the function |
| IsPure | boolean | No | Whether the function is pure (no side effects, default: false) |
| Category | string | No | Category for organizing in the function list |
| AccessSpecifier | string | No | "Public", "Protected", or "Private" (default: "Public") |

## Examples

### Create Basic Function
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"FunctionName\": \"TakeDamage\"}"
}
```

### Create Pure Function
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Math\", \"FunctionName\": \"CalculateDistance\", \"IsPure\": true, \"Description\": \"Calculate distance between two points\"}"
}
```

### Create Categorized Function
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Character\", \"FunctionName\": \"RestoreHealth\", \"Category\": \"Health\", \"AccessSpecifier\": \"Public\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "FunctionName": "TakeDamage",
  "Message": "Function created successfully"
}
```

## Tips

- Function names should use PascalCase convention
- Pure functions can be called multiple times without side effects and are optimized by the compiler
- Use categories to organize functions in complex Blueprints
- Private functions can only be called within the same Blueprint
- After creating, use `add_input` and `add_output` to define parameters
