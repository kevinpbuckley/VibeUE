# disconnect

Disconnect pins from each other, removing a wire.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the graph |
| NodeId | string | Yes | ID of the node with the pin to disconnect |
| PinName | string | Yes | Name of the pin to disconnect |
| TargetNodeId | string | No | Specific target node to disconnect from (disconnects all if not specified) |

## Examples

### Disconnect Specific Connection
```json
{
  "Action": "disconnect",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeId\": \"Branch_0\", \"PinName\": \"True\", \"TargetNodeId\": \"PrintString_0\"}"
}
```

### Disconnect All from Pin
```json
{
  "Action": "disconnect",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeId\": \"GetHealth\", \"PinName\": \"ReturnValue\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "GraphName": "EventGraph",
  "NodeId": "Branch_0",
  "PinName": "True",
  "DisconnectedCount": 1,
  "Message": "Pin disconnected successfully"
}
```

## Tips

- Without TargetNodeId, all connections from the pin are removed
- Disconnecting required pins may cause compilation errors
- Use this to rewire node connections
- Check graph with `list` action after disconnecting
