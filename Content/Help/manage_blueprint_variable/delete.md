# delete

Delete a variable from a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| VariableName | string | Yes | Name of the variable to delete |

## Examples

### Delete Variable
```json
{
  "Action": "delete",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"VariableName\": \"OldUnusedVariable\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "VariableName": "OldUnusedVariable",
  "Message": "Variable deleted successfully"
}
```

## Tips

- Deleting a variable breaks all Get/Set nodes referencing it
- Cannot delete inherited variables from parent class
- Consider searching for usages before deleting
- Compile the Blueprint after deletion to see any errors
- Deleted variables are removed from instance customization in the level
