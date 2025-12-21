# delete

Delete a function from a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| FunctionName | string | Yes | Name of the function to delete |

## Examples

### Delete Function
```json
{
  "Action": "delete",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"FunctionName\": \"OldUnusedFunction\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "FunctionName": "OldUnusedFunction",
  "Message": "Function deleted successfully"
}
```

## Tips

- Deleting a function will break any nodes that call it
- Cannot delete inherited functions or event overrides
- Consider checking for usages before deleting
- Compile the Blueprint after deletion to validate
- The function's graph and all nodes within it are removed
