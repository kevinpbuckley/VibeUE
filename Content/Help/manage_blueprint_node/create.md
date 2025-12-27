# create

Create a new node in a Blueprint graph.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the graph ("EventGraph" or function name) |
| NodeType | string | Yes | Type of node to create (from discover action) |
| Position | object | No | Position in graph {"X": number, "Y": number} |
| NodeName | string | No | Custom name/identifier for the node |

## Examples

### Create Print String Node
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeType\": \"PrintString\", \"Position\": {\"X\": 300, \"Y\": 100}}"
}
```

### Create Branch Node
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeType\": \"Branch\", \"Position\": {\"X\": 400, \"Y\": 200}}"
}
```

### Create Variable Get Node
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeType\": \"VariableGet\", \"VariableName\": \"Health\"}"
}
```

### Create Function Call Node
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"TakeDamage\", \"NodeType\": \"CallFunction\", \"FunctionName\": \"GetActorLocation\"}"
}
```

### Create Event Node
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeType\": \"Event\", \"EventName\": \"EventTick\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "GraphName": "EventGraph",
  "NodeId": "K2Node_CallFunction_0",
  "NodeType": "PrintString",
  "Position": {"X": 300, "Y": 100},
  "Message": "Node created successfully"
}
```

## Tips

- Use `discover` action to find valid NodeType values
- Position is in graph coordinates (optional, auto-placed if not specified)
- NodeId is returned and needed for connect/configure operations
- For variable nodes, specify VariableName parameter
- For function calls, specify FunctionName or use full function reference
- Compile after creating nodes to validate connections
