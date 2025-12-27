# recombine

Recombine previously split pins back into a single struct pin.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the graph |
| NodeId | string | Yes | ID of the node with the split pin |
| PinName | string | Yes | Base name of the split pin to recombine |

## Examples

### Recombine Vector Pin
```json
{
  "Action": "recombine",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeId\": \"GetActorLocation_0\", \"PinName\": \"ReturnValue\"}"
}
```

### Recombine Transform Pin
```json
{
  "Action": "recombine",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Spawner\", \"GraphName\": \"SpawnActor\", \"NodeId\": \"SpawnActor_0\", \"PinName\": \"SpawnTransform\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "GraphName": "EventGraph",
  "NodeId": "GetActorLocation_0",
  "RecombinedPin": "ReturnValue",
  "Message": "Pin recombined successfully"
}
```

## Tips

- Only works on pins that were previously split
- Connections to individual components are disconnected
- Use to simplify the graph when component access is no longer needed
- The recombined pin can then be connected to other struct pins
