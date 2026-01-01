# manage_blueprint_function

Manage functions within Blueprints - create, delete, and configure function parameters.

## Summary

The `manage_blueprint_function` tool allows you to create and manage custom functions in Blueprint assets. Functions are reusable blocks of logic that can have input and output parameters. This tool lets you create new functions, add/remove parameters, and query function information.

## Actions

| Action | Description |
|--------|-------------|
| create | Create a new function in a Blueprint |
| delete | Delete an existing function from a Blueprint |
| get_info | Get detailed information about a function |
| add_input | Add an input parameter to a function |
| add_output | Add an output parameter (return value) to a function |
| remove_param | Remove a parameter from a function |
| list | List all functions in a Blueprint |
| add_local_variable | Add a local variable to a function |
| remove_local_variable | Remove a local variable from a function |
| update_local_variable | Update properties of an existing local variable |
| list_local_variables | List all local variables in a function |
| get_available_local_types | Get list of available types for local variables |

## Action Selection Guide

**When to use each action:**

- **add_local_variable**: Creating a NEW local variable that doesn't exist yet
- **update_local_variable**: Changing properties of an EXISTING local variable (type, default value, category)
- **remove_local_variable**: Deleting an existing local variable

**Common patterns:**
- "Add a variable X" → use `add_local_variable`
- "Change variable X to type Y" → use `update_local_variable` (NOT add_local_variable)
- "Update variable X" → use `update_local_variable`
- "Remove/delete variable X" → use `remove_local_variable`
- "List variables" → use `list_local_variables`

**Important:** To modify an existing variable's type or properties, always use `update_local_variable`, not `add_local_variable`. Adding a variable with the same name as an existing one will fail.

## Usage

### Create a Function
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"FunctionName\": \"CalculateDamage\", \"Description\": \"Calculate damage based on weapon and modifiers\"}"
}
```

### Add Input Parameter
```json
{
  "Action": "add_input",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"FunctionName\": \"CalculateDamage\", \"ParamName\": \"BaseDamage\", \"ParamType\": \"Float\"}"
}
```

### Add Output Parameter
```json
{
  "Action": "add_output",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"FunctionName\": \"CalculateDamage\", \"ParamName\": \"FinalDamage\", \"ParamType\": \"Float\"}"
}
```

## Local Variables

Local variables are private variables scoped to a single function. Unlike function parameters, they:
- Don't appear as pins on function call nodes
- Are accessed via Get/Set variable nodes within the function graph
- Persist during the function execution but reset between calls
- Can be used to store intermediate calculation results

### Working with Local Variables
```json
// Get available types for local variables
{
  "Action": "get_available_local_types"
}

// Add a local variable
{
  "Action": "add_local_variable",
  "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "CalculateHealth", "local_name": "TempResult", "type": "float"}
}

// List local variables
{
  "Action": "list_local_variables",
  "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "CalculateHealth"}
}

// Remove a local variable
{
  "Action": "remove_local_variable",
  "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "CalculateHealth", "local_name": "TempResult"}
}

// Update a local variable's type
{
  "Action": "update_local_variable",
  "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "CalculateHealth", "local_name": "TempResult", "type": "int"}
}
```

## Type Aliases

When specifying parameter or local variable types, you can use these aliases:
- `float` or `Float` - Single precision floating point
- `int` or `Integer` or `int32` - 32-bit integer  
- `bool` or `Boolean` - Boolean value
- `string` or `String` - Text string

For complex types, use the exact class name (e.g., `Vector`, `Rotator`, `Transform`, `LinearColor`).

## Notes

- Function names must be unique within a Blueprint
- Functions should be compiled after modification
- Pure functions (no side effects) can be marked for optimization
- Use `manage_blueprint_node` to add logic inside functions
- After adding/removing parameters or local variables, compile the Blueprint
