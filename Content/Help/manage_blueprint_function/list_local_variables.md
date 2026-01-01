# list_local_variables

List all local variables in a function.

## Aliases

- `list_locals`
- `list_local_vars`
- `locals`

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| blueprint_name | string | Yes | Name of the Blueprint (short name, not full path) |
| function_name | string | Yes | Name of the function to query |

## Examples

### List Local Variables
```json
{
  "Action": "list_local_variables",
  "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "CalculateHealth"}
}
```

### Using Alias
```json
{
  "Action": "list_locals",
  "ParamsJson": {"blueprint_name": "BP_Enemy", "function_name": "Attack"}
}
```

## Returns

Success response with array of local variables:
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
    },
    {
      "name": "Counter",
      "type": "int",
      "default": "0",
      "is_const": false,
      "is_reference": false
    }
  ],
  "count": 2
}
```

Empty function response:
```json
{
  "success": true,
  "function_name": "EmptyFunction",
  "locals": [],
  "count": 0
}
```

## Notes

- Local variables are private to the function scope
- They don't appear as pins on function call nodes
- Use Get/Set variable nodes within the function to access local variables
- Local variables persist across multiple executions of the function during a single frame
