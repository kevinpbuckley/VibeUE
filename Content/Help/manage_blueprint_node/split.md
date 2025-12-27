# split

Split a struct pin into individual member pins for granular access.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the graph |
| NodeId | string | Yes | ID of the node with the struct pin |
| PinName | string | Yes | Name of the struct pin to split |

## Examples

### Split Vector Pin
```json
{
  "Action": "split",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeId\": \"GetActorLocation_0\", \"PinName\": \"ReturnValue\"}"
}
```

### Split Transform Pin
```json
{
  "Action": "split",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Spawner\", \"GraphName\": \"SpawnActor\", \"NodeId\": \"SpawnActor_0\", \"PinName\": \"SpawnTransform\"}"
}
```

### Split Rotator Pin
```json
{
  "Action": "split",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Camera\", \"GraphName\": \"EventGraph\", \"NodeId\": \"GetControlRotation_0\", \"PinName\": \"ReturnValue\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "GraphName": "EventGraph",
  "NodeId": "GetActorLocation_0",
  "OriginalPin": "ReturnValue",
  "SplitPins": [
    {"Name": "ReturnValue_X", "Type": "Float"},
    {"Name": "ReturnValue_Y", "Type": "Float"},
    {"Name": "ReturnValue_Z", "Type": "Float"}
  ],
  "Message": "Pin split successfully"
}
```

## Tips

- Common splittable types: Vector (X,Y,Z), Rotator (Pitch,Yaw,Roll), Transform, Color (R,G,B,A)
- Split pins allow connecting to individual components
- Not all struct pins can be split
- Use `recombine` to undo a split
- Existing connections are preserved if compatible
