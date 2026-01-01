# details

Get detailed information about a specific node including all pins and connections.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the graph |
| NodeId | string | Yes | ID of the node to inspect |

## Examples

### Get Node Details
```json
{
  "Action": "details",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeId\": \"K2Node_CallFunction_0\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "GraphName": "EventGraph",
  "Node": {
    "NodeId": "K2Node_CallFunction_0",
    "NodeType": "CallFunction",
    "DisplayName": "Print String",
    "GraphName": "EventGraph",
    "Category": "Utilities|String",
    "Position": {"X": 300, "Y": 0},
    "InputPins": [
      {
        "Name": "execute",
        "Type": "exec",
        "IsConnected": true,
        "ConnectedTo": ["K2Node_Event_0.then"]
      },
      {
        "Name": "InString",
        "Type": "String",
        "IsConnected": true,
        "ConnectedTo": ["K2Node_VariableGet_0.Health"],
        "DefaultValue": "Hello"
      },
      {
        "Name": "bPrintToScreen",
        "Type": "Boolean",
        "IsConnected": false,
        "DefaultValue": true
      }
    ],
    "OutputPins": [
      {
        "Name": "then",
        "Type": "exec",
        "IsConnected": false,
        "ConnectedTo": []
      }
    ]
  }
}
```

## Tips

- Shows all pins with their types and connections
- DefaultValue shows values set directly on the node
- ConnectedTo shows what each pin is wired to
- Use this to understand node structure before connecting
- Exec pins handle flow control, data pins transfer values
- **GraphName field shows which graph the node belongs to**
  - Verifies the node is in the expected graph
  - Function nodes show function name (e.g., "CalculateHealth")
  - Event nodes show "EventGraph"
