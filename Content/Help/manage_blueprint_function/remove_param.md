# remove_param

Remove an input or output parameter from a function.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| FunctionName | string | Yes | Name of the function to modify |
| ParamName | string | Yes | Name of the parameter to remove |

## Examples

### Remove Parameter
```json
{
  "Action": "remove_param",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"FunctionName\": \"TakeDamage\", \"ParamName\": \"ObsoleteParam\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "FunctionName": "TakeDamage",
  "ParamName": "ObsoleteParam",
  "Message": "Parameter removed successfully"
}
```

## Tips

- Removing a parameter disconnects any wires connected to that pin
- All call sites of this function may need updating
- Works for both input and output parameters
- Cannot remove parameters from inherited/overridden functions
- Compile the Blueprint after removing parameters to validate
