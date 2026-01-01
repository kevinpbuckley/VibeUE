# manage_blueprint_node

Manage Blueprint graph nodes - discover, create, delete, connect, and configure nodes within Blueprint functions and event graphs.

## Summary

The `manage_blueprint_node` tool provides comprehensive node manipulation capabilities for Blueprint visual scripting. It allows you to discover available node types, create new nodes, connect them together, configure their properties, and manage the overall graph structure.

## Actions

| Action | Description |
|--------|-------------|
| discover | Discover available node types that can be created (supports pin-type filtering) |
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

## Pin-Type-Sensitive Discovery

The `discover` action supports **pin-type-sensitive filtering** - like Unreal's "drag from pin" context menu. Pass `source_node_id` and `source_pin` to filter results to only nodes that can connect to that pin type.

### Example: Find Nodes Compatible with a Float Output
```json
{
  "Action": "discover",
  "ParamsJson": "{\"blueprint_name\": \"BP_Player\", \"search_term\": \"print\", \"source_node_id\": \"{NODE_GUID}\", \"source_pin\": \"ReturnValue\"}"
}
```

This returns only nodes with pins compatible with the source pin's type.

## Usage

### Discover Node Types
```json
{
  "Action": "discover",
  "ParamsJson": "{\"blueprint_name\": \"BP_Player\", \"search_term\": \"Print\", \"category_filter\": \"Utilities\"}"
}
```

### Create a Node
```json
{
  "Action": "create",
  "ParamsJson": "{\"blueprint_name\": \"BP_Player\", \"spawner_key\": \"UKismetSystemLibrary::PrintString(...)\", \"position\": [200, 100]}"
}
```

### Connect Nodes
```json
{
  "Action": "connect",
  "ParamsJson": "{\"blueprint_name\": \"BP_Player\", \"source_node_id\": \"{SOURCE_GUID}\", \"source_pin\": \"exec\", \"target_node_id\": \"{TARGET_GUID}\", \"target_pin\": \"execute\"}"
}
```

## Notes

- Node operations require specifying the graph (EventGraph, function name, etc.)
- Pin names are case-sensitive
- Always compile after making node changes
- Use `discover` to find available nodes for specific tasks
- Use `source_node_id` + `source_pin` in discover for pin-type-sensitive filtering

## Connection Workflow Guide

When connecting nodes, follow this workflow to avoid common errors:

### Step 1: List nodes to get IDs
```json
{"Action": "list", "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "MyFunction"}}
```

### Step 2: Get details for exact pin names
```json
{"Action": "details", "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "MyFunction", "node_id": "{NODE_GUID}"}}
```

### Step 3: Connect with exact pin names
```json
{"Action": "connect", "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "MyFunction", "source_node_id": "{SOURCE_GUID}", "source_pin": "ExactPinName", "target_node_id": "{TARGET_GUID}", "target_pin": "ExactPinName"}}
```

### CRITICAL: NO Conversion Nodes Needed Between Float/Real/Double
**Unreal Engine automatically converts between float, real, and double types when connecting pins.**

**DO NOT search for conversion nodes** like:
- "Float to Real"
- "Float to Double"  
- "Cast Float"
- "Convert Float"

These **DO NOT EXIST** as separate nodes. Just use the `connect` action directly - Unreal handles implicit type conversion automatically.

### Wildcard Pin Warning (UE5 Type System Issue)
Math operators (Multiply, Add, Subtract, Divide) created via MCP have **wildcard pins** that expect double-precision ('real') types. Blueprint float variables are single-precision, so connecting them directly will fail with "No matching 'Multiply' function for 'Float'".

**Workarounds:**
- Use typed math function nodes: `Clamp_(Float)`, `Lerp_(float)`, `Power_Float`
- Use Double variables instead of Float in your Blueprint
