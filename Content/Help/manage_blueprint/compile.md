# compile

Compile a Blueprint to update its generated class and validate its logic.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint to compile |

## Examples

### Compile Blueprint
```json
{
  "Action": "compile",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Enemy\"}"
}
```

## Returns

### Success
```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Enemy",
  "Message": "Blueprint compiled successfully"
}
```

### Compilation Errors
```json
{
  "Success": false,
  "BlueprintPath": "/Game/Blueprints/BP_Enemy",
  "Errors": [
    "Error: Node 'GetHealth' has disconnected output pin",
    "Warning: Variable 'OldVariable' is unused"
  ],
  "Message": "Blueprint compilation failed with errors"
}
```

## Tips

- Always compile after making changes to variables, functions, or node connections
- Compilation validates all Blueprint logic and node connections
- Uncompiled Blueprints may have stale generated classes
- Errors must be resolved before the Blueprint can be used
- Warnings don't prevent compilation but should be addressed
- Compile is automatically triggered when saving in the editor
