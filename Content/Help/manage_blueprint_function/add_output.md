# add_output

Add an output parameter (return value) to a function.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| blueprint_name | string | Yes | Name of the Blueprint (short name, not full path) |
| function_name | string | Yes | Name of the function to modify |
| param_name | string | Yes | Name for the new output parameter |
| param_type | string | Yes | Type of the parameter (e.g., "Float", "Integer", "Boolean", "String", "Vector", "LinearColor") |

## Examples

### Add Float Output
```json
{
  "Action": "add_output",
  "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "GetHealth", "param_name": "CurrentHealth", "param_type": "Float"}
}
```

### Add Boolean Output
```json
{
  "Action": "add_output",
  "ParamsJson": {"blueprint_name": "BP_Enemy", "function_name": "IsAlive", "param_name": "bAlive", "param_type": "Boolean"}
}
```

### Add LinearColor Output
```json
{
  "Action": "add_output",
  "ParamsJson": {"blueprint_name": "BP_Widget", "function_name": "SetColors", "param_name": "OutColor", "param_type": "LinearColor"}
}
```

### Add Multiple Outputs (call multiple times)
```json
{
  "Action": "add_output",
  "ParamsJson": {"blueprint_name": "BP_Inventory", "function_name": "GetItemInfo", "param_name": "ItemName", "param_type": "String"}
}
```

## Returns

```json
{
  "success": true,
  "blueprint_name": "BP_Player",
  "function_name": "GetHealth",
  "param_name": "CurrentHealth",
  "param_type": "Float",
  "message": "Output parameter added successfully"
}
```

## Tips

- Functions can have multiple output parameters (call this action multiple times)
- Output parameters appear as pins on the Return Node
- Common types: Float, Integer, Boolean, String, Name, Text, Vector, Rotator, Transform, LinearColor, Actor, Object
- For color outputs, use "LinearColor" as the type
- The first output is typically the "main" return value
- For simple returns, name the output "ReturnValue"
