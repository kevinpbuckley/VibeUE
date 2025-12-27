# configure

Configure node settings and advanced options.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the graph |
| NodeId | string | Yes | ID of the node to configure |
| Settings | object | Yes | Configuration settings to apply |

## Examples

### Configure Delay Node
```json
{
  "Action": "configure",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeId\": \"Delay_0\", \"Settings\": {\"Duration\": 2.0}}"
}
```

### Configure For Loop
```json
{
  "Action": "configure",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Enemy\", \"GraphName\": \"SpawnWave\", \"NodeId\": \"ForLoop_0\", \"Settings\": {\"FirstIndex\": 0, \"LastIndex\": 10}}"
}
```

### Configure Enum Switch
```json
{
  "Action": "configure",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Game\", \"GraphName\": \"EventGraph\", \"NodeId\": \"SwitchEnum_0\", \"Settings\": {\"EnumType\": \"EGameState\"}}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "GraphName": "EventGraph",
  "NodeId": "Delay_0",
  "AppliedSettings": {
    "Duration": 2.0
  },
  "Message": "Node configured successfully"
}
```

## Tips

- Configuration options vary by node type
- Some nodes require configuration before they can be connected
- Use `details` action to see available configuration options
- Switches need their enum/type configured first
- Compile after configuration changes
