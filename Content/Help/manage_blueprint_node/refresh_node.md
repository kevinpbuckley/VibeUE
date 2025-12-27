# refresh_node

Refresh a node to update its pins and connections after changes.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the graph |
| NodeId | string | Yes | ID of the node to refresh |

## Examples

### Refresh Function Call Node
```json
{
  "Action": "refresh_node",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeId\": \"K2Node_CallFunction_3\"}"
}
```

### Refresh Custom Event Node
```json
{
  "Action": "refresh_node",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Game\", \"GraphName\": \"EventGraph\", \"NodeId\": \"CustomEvent_OnGameStart\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "GraphName": "EventGraph",
  "NodeId": "K2Node_CallFunction_3",
  "PinsUpdated": true,
  "Message": "Node refreshed successfully"
}
```

## Tips

- Refresh nodes when their function signature has changed
- Useful after modifying custom functions or events
- Disconnected pins may result if signatures don't match
- Required when upgrading Blueprints to new engine versions
- Compile after refreshing to validate the Blueprint
