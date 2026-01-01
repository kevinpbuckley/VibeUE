# remove_local_variable

Remove a local variable from a function.

## Aliases

- `remove_local`
- `remove_local_var`

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| blueprint_name | string | Yes | Name of the Blueprint (short name, not full path) |
| function_name | string | Yes | Name of the function containing the local variable |
| local_name | string | Yes | Name of the local variable to remove (also accepts variable_name or param_name) |

## Examples

### Remove Local Variable
```json
{
  "Action": "remove_local_variable",
  "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "CalculateHealth", "local_name": "TempResult"}
}
```

### Remove Using Alias
```json
{
  "Action": "remove_local",
  "ParamsJson": {"blueprint_name": "BP_Enemy", "function_name": "Attack", "local_name": "DamageMultiplier"}
}
```

## Returns

Success response:
```json
{
  "success": true,
  "function_name": "CalculateHealth",
  "removed_variable": "TempResult",
  "message": "Local variable 'TempResult' removed successfully"
}
```

Error response if variable not found:
```json
{
  "success": false,
  "error": "Local variable 'TempResult' not found"
}
```

## Notes

- Removing a local variable will break any Get/Set nodes that reference it
- Ensure no nodes are using the variable before removing it
- After removing local variables, compile the Blueprint to update the graph
- You can use `list_local_variables` to see all local variables before removing
