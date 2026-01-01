# add_local_variable

Add a local variable to a function.

## Aliases

- `add_local`
- `add_local_var`

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| blueprint_name | string | Yes | Name of the Blueprint (short name, not full path) |
| function_name | string | Yes | Name of the function to modify |
| local_name | string | Yes | Name for the new local variable (also accepts param_name, variable_name, or name) |
| type | string | Yes | Type of the variable (also accepts local_type or variable_type) |
| default_value | any | No | Default value for the variable |
| is_const | boolean | No | Whether the variable is const (default: false) |
| is_reference | boolean | No | Whether the variable is a reference (default: false) |

## Type Aliases

Common type names that work:
- `float` or `Float` - Single precision floating point
- `int` or `Integer` or `int32` - 32-bit integer
- `bool` or `Boolean` - Boolean value
- `string` or `String` - Text string
- `Vector` - 3D vector
- `Rotator` - Rotation
- `Transform` - Transform (location, rotation, scale)
- `LinearColor` - RGBA color

## Examples

### Add Float Local Variable
```json
{
  "Action": "add_local_variable",
  "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "CalculateHealth", "local_name": "TempResult", "type": "float"}
}
```

### Add Integer Local Variable
```json
{
  "Action": "add_local_variable",
  "ParamsJson": {"blueprint_name": "BP_Enemy", "function_name": "CalculateDamage", "local_name": "Multiplier", "type": "int", "default_value": "1"}
}
```

### Add Boolean Local Variable
```json
{
  "Action": "add_local_variable",
  "ParamsJson": {"blueprint_name": "BP_Widget", "function_name": "UpdateUI", "local_name": "bShouldUpdate", "type": "bool", "default_value": "true"}
}
```

### Add Vector Local Variable
```json
{
  "Action": "add_local_variable",
  "ParamsJson": {"blueprint_name": "BP_Projectile", "function_name": "CalculateTrajectory", "local_name": "TempDirection", "type": "Vector"}
}
```

## Returns

Success response with the updated list of local variables:
```json
{
  "success": true,
  "function_name": "CalculateHealth",
  "locals": [
    {
      "name": "TempResult",
      "type": "float",
      "default": "0.0",
      "is_const": false,
      "is_reference": false
    }
  ],
  "count": 1
}
```

## Notes

- Local variables are scoped to the function and are not visible outside it
- Unlike function parameters, local variables don't appear as pins on the function node
- Local variables can be accessed using Get/Set variable nodes within the function graph
- Type must be specified exactly - use lowercase for primitive types (float, int, bool)
- After adding local variables, compile the Blueprint to ensure changes take effect
- **Tip**: Use `get_available_local_types` action to discover all available type descriptors before adding variables
