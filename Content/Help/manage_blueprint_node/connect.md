# connect

Connect two node pins together to create a wire.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| blueprint_name | string | Yes | Name or path of the Blueprint |
| source_node_id | string | Yes | ID of the source node (output) |
| source_pin | string | Yes | Name of the output pin on source node |
| target_node_id | string | Yes | ID of the target node (input) |
| target_pin | string | Yes | Name of the input pin on target node |
| function_name | string | No | Name of function graph (for function nodes) |

## Common Connection Issues

### IMPORTANT: Automatic Type Conversion
**Unreal Engine performs AUTOMATIC type conversion when connecting pins** - you DO NOT need separate conversion nodes!

When you connect a `float` pin to a `real` (double-precision) pin, Unreal automatically inserts an implicit conversion. **DO NOT search for conversion nodes** like "Float to Real", "Float to Double", "Cast Float", etc. - these DO NOT EXIST as separate nodes.

**Just connect directly** using the `connect` action - Unreal handles the conversion transparently.

Example: Connecting a float variable to Clamp's real input:
```json
{
  "Action": "connect",
  "ParamsJson": {
    "blueprint_name": "BP_Player",
    "source_node_id": "{FLOAT_VAR_NODE_ID}",
    "source_pin": "Health",
    "target_node_id": "{CLAMP_NODE_ID}",
    "target_pin": "Value"
  }
}
```
No conversion node needed - Unreal converts float→real automatically!

### Math Operator Wildcard Errors
**Error:** "No matching 'Multiply' function for 'Float (single-precision)'"

This is a **UE5 type system limitation**. Math operators (Multiply, Add, Subtract, Divide) created via MCP have **wildcard pins** that expect double-precision ('real') types, but Blueprint float variables are single-precision.

**Solutions:**
1. **Use typed function nodes instead** - Search for specific math functions:
   - `Clamp_(Float)` - Clamp a float value
   - `Lerp_(float)` - Linear interpolation  
   - `Power_Float` - Exponentiation
   - These have properly typed pins that work with single-precision floats

2. **Use Blueprint Double variables** - When creating variables, use Double type instead of Float

3. **Alternative math approaches:**
   - For simple scaling: Use `Lerp` with 0 and the scale factor
   - For power operations: Use `Power_Float`
   - For clamping: Use `Clamp_(Float)`

### Pin Not Found
**Error:** "Pin 'A' not found on node"

Pin names are case-sensitive and may differ from display names.

**Solution:** Always use `details` action first to get exact pin names.

## Recommended Workflow

Before connecting nodes, always:
1. Use `list` action to see all nodes in the graph with their IDs
2. Use `details` action on both source and target nodes to get exact pin names and types
3. Check if any pins are "wildcard" type - connect typed pins to them first
4. Make the connection with exact pin names (case-sensitive)

## Examples

### Connect Execution Pins
```json
{
  "Action": "connect",
  "ParamsJson": {"blueprint_name": "BP_Player", "source_node_id": "EventBeginPlay", "source_pin": "then", "target_node_id": "PrintString_0", "target_pin": "execute"}
}
```

### Connect Data Pins
```json
{
  "Action": "connect",
  "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "CalculateHealth", "source_node_id": "{GUID}", "source_pin": "ReturnValue", "target_node_id": "{GUID}", "target_pin": "A"}
}
```

### Connect to Math Node (Wildcard Handling)
```json
// Step 1: Get node details to see pin names
{"Action": "details", "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "MyFunction", "node_id": "{MULTIPLY_NODE_GUID}"}}

// Step 2: Connect typed source to resolve wildcard
{"Action": "connect", "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "MyFunction", "source_node_id": "{FLOAT_PARAM_NODE}", "source_pin": "BaseHealth", "target_node_id": "{MULTIPLY_NODE_GUID}", "target_pin": "A"}}
```

## Returns

```json
{
  "success": true,
  "blueprint_name": "BP_Player",
  "connections": [
    {
      "source_node_id": "{GUID}",
      "source_pin_name": "then",
      "target_node_id": "{GUID}",
      "target_pin_name": "execute"
    }
  ]
}
```

## Tips

- **Always use `details` first** to get exact pin names before connecting
- Execution pins are typically named "execute" (input) and "then" (output)
- Data pins must have compatible types (or implicit conversion available)
- Pin names are case-sensitive - use exact names from `details`
- White pins are execution flow, colored pins are data
- For function graphs, include `function_name` parameter
- **For wildcard pins (math operators):** Connect a typed pin first to resolve the type
