# connect

Connect two node pins together to create a wire.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the graph |
| SourceNodeId | string | Yes | ID of the source node |
| SourcePin | string | Yes | Name of the output pin on source node |
| TargetNodeId | string | Yes | ID of the target node |
| TargetPin | string | Yes | Name of the input pin on target node |

## Examples

### Connect Execution Pins
```json
{
  "Action": "connect",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"SourceNodeId\": \"EventBeginPlay\", \"SourcePin\": \"then\", \"TargetNodeId\": \"PrintString_0\", \"TargetPin\": \"execute\"}"
}
```

### Connect Data Pins
```json
{
  "Action": "connect",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"SourceNodeId\": \"GetHealth\", \"SourcePin\": \"ReturnValue\", \"TargetNodeId\": \"PrintString_0\", \"TargetPin\": \"InString\"}"
}
```

### Connect Branch Output
```json
{
  "Action": "connect",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"TakeDamage\", \"SourceNodeId\": \"Branch_0\", \"SourcePin\": \"True\", \"TargetNodeId\": \"DestroyActor\", \"TargetPin\": \"execute\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "GraphName": "EventGraph",
  "Connection": {
    "Source": "EventBeginPlay.then",
    "Target": "PrintString_0.execute"
  },
  "Message": "Pins connected successfully"
}
```

## Tips

- Execution pins are typically named "execute" (input) and "then" (output)
- Data pins must have compatible types (or implicit conversion available)
- Pin names are case-sensitive
- Use `details` action to discover available pins on a node
- White pins are execution flow, colored pins are data
- Multiple wires can connect to a single input (for exec) or output (for data)
