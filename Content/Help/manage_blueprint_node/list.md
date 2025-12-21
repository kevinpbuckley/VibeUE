# list

List all nodes in a Blueprint function or event graph.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the graph to list ("EventGraph" or function name) |

## Examples

### List EventGraph Nodes
```json
{
  "Action": "list",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\"}"
}
```

### List Function Nodes
```json
{
  "Action": "list",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"CalculateDamage\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "GraphName": "EventGraph",
  "Nodes": [
    {
      "NodeId": "K2Node_Event_0",
      "NodeType": "Event",
      "DisplayName": "Event BeginPlay",
      "Position": {"X": 0, "Y": 0}
    },
    {
      "NodeId": "K2Node_CallFunction_0",
      "NodeType": "CallFunction",
      "DisplayName": "Print String",
      "Position": {"X": 300, "Y": 0}
    },
    {
      "NodeId": "K2Node_VariableGet_0",
      "NodeType": "VariableGet",
      "DisplayName": "Get Health",
      "Position": {"X": 100, "Y": 100}
    }
  ],
  "NodeCount": 3
}
```

## Tips

- NodeId is used for other operations (connect, delete, etc.)
- EventGraph contains event implementations
- Function names show their specific graph nodes
- Use `details` action for full information about specific nodes
