# list

List all nodes in a Blueprint function or event graph.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the target graph. Can be "EventGraph" (main event graph), a function graph name, or macro graph name. Leave empty to default to EventGraph. |

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
      "GraphName": "EventGraph",
      "Position": {"X": 0, "Y": 0}
    },
    {
      "NodeId": "K2Node_CallFunction_0",
      "NodeType": "CallFunction",
      "DisplayName": "Print String",
      "GraphName": "EventGraph",
      "Position": {"X": 300, "Y": 0}
    },
    {
      "NodeId": "K2Node_VariableGet_0",
      "NodeType": "VariableGet",
      "DisplayName": "Get Health",
      "GraphName": "EventGraph",
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
- **Each node includes a GraphName field showing which graph it belongs to**
  - Useful for verifying nodes were created in the correct graph
  - Function nodes will show the function name (e.g., "CalculateHealth")
  - Event nodes will show "EventGraph"
- **GraphName parameter works with Event Graphs, Function Graphs, and Macro Graphs**
  - The system automatically searches all graph types when you provide a name
