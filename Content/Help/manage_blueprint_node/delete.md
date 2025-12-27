# delete

Remove a node from a Blueprint graph.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the graph containing the node |
| NodeId | string | Yes | ID of the node to delete |

## Examples

### Delete Node
```json
{
  "Action": "delete",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeId\": \"K2Node_CallFunction_5\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "GraphName": "EventGraph",
  "NodeId": "K2Node_CallFunction_5",
  "Message": "Node deleted successfully"
}
```

## Tips

- Deleting a node removes all connections to/from it
- Use `list` action to find node IDs
- Cannot delete required nodes (like function entry/exit)
- Event nodes in EventGraph can be deleted (they can be recreated)
- Compile after deletion to check for broken logic
