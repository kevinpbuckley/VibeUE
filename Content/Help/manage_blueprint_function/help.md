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

## Notes

- Function names must be unique within a Blueprint
- Functions should be compiled after modification
- Pure functions (no side effects) can be marked for optimization
- Use `manage_blueprint_node` to add logic inside functions
