# discover

Discover available Blueprint node types that can be created.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| SearchTerm | string | No | Search term to filter node types |
| Category | string | No | Filter by category (e.g., "Math", "Flow Control", "Utilities") |
| ContextClass | string | No | Class context for available functions (e.g., "Actor", "PlayerController") |
| MaxResults | number | No | Maximum number of results to return (default: 50) |

## Examples

### Search for Print Nodes
```json
{
  "Action": "discover",
  "ParamsJson": "{\"SearchTerm\": \"Print\"}"
}
```

### Find Math Nodes
```json
{
  "Action": "discover",
  "ParamsJson": "{\"Category\": \"Math\"}"
}
```

### Find Actor Functions
```json
{
  "Action": "discover",
  "ParamsJson": "{\"ContextClass\": \"Actor\", \"SearchTerm\": \"Location\"}"
}
```

### Find Flow Control Nodes
```json
{
  "Action": "discover",
  "ParamsJson": "{\"Category\": \"Flow Control\"}"
}
```

## Returns

```json
{
  "Success": true,
  "Nodes": [
    {
      "NodeType": "K2Node_CallFunction",
      "FunctionName": "PrintString",
      "Category": "Utilities|String",
      "Description": "Prints a string to the output log and optionally to the screen",
      "Keywords": ["print", "log", "debug"]
    },
    {
      "NodeType": "K2Node_CallFunction",
      "FunctionName": "PrintText",
      "Category": "Utilities|Text",
      "Description": "Prints a text value to the log",
      "Keywords": ["print", "log", "text"]
    }
  ],
  "Count": 2
}
```

## Tips

- Use broad search terms to discover related functionality
- Categories help narrow down to specific areas
- ContextClass shows functions available on that class type
- Keywords in results can help find related nodes
- Common categories: Math, Flow Control, Utilities, String, Array, Transform
