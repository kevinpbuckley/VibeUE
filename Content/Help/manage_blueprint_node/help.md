# manage_blueprint_node

Manage Blueprint graph nodes - discover, create, delete, connect, and configure nodes within Blueprint functions and event graphs.

## Summary

The `manage_blueprint_node` tool provides comprehensive node manipulation capabilities for Blueprint visual scripting. It allows you to discover available node types, create new nodes, connect them together, configure their properties, and manage the overall graph structure.

## Actions

| Action | Description |
|--------|-------------|
| discover | Discover available node types that can be created |
| create | Create a new node in a Blueprint graph |
| delete | Remove a node from the graph |
| connect | Connect two node pins together |
| disconnect | Disconnect pins from each other |
| list | List all nodes in a function or event graph |
| details | Get detailed information about a specific node |
| set_property | Set a property value on a node |
| configure | Configure node settings and options |
| split | Split a struct pin into individual member pins |
| recombine | Recombine split pins back into a struct pin |
| refresh_node | Refresh a node to update its pins and connections |

## Usage

### Discover Node Types
```json
{
  "Action": "discover",
  "ParamsJson": "{\"SearchTerm\": \"Print\", \"Category\": \"Utilities\"}"
}
```

### Create a Node
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeType\": \"PrintString\", \"Position\": {\"X\": 200, \"Y\": 100}}"
}
```

### Connect Nodes
```json
{
  "Action": "connect",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"SourceNode\": \"EventBeginPlay\", \"SourcePin\": \"exec\", \"TargetNode\": \"PrintString\", \"TargetPin\": \"execute\"}"
}
```

## Notes

- Node operations require specifying the graph (EventGraph, function name, etc.)
- Pin names are case-sensitive
- Always compile after making node changes
- Use `discover` to find available nodes for specific tasks
