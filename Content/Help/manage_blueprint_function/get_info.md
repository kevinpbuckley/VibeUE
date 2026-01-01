# get_info

Get detailed information about a specific function in a Blueprint, including parameters and local variables.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| blueprint_name | string | Yes | Name of the Blueprint (short name, not full path) |
| function_name | string | Yes | Name of the function to inspect |

## Examples

### Get Function Info
```json
{
  "Action": "get_info",
  "ParamsJson": {"blueprint_name": "BP_Player", "function_name": "CalculateDamage"}
}
```

## Returns

Detailed function information including parameters and local variables:
```json
{
  "name": "CalculateDamage",
  "node_count": 5,
  "graph_guid": "A1B2C3D4-5678-90AB-CDEF-1234567890AB",
  "parameters": [
    {
      "name": "BaseDamage",
      "direction": "input",
      "type": "float"
    },
    {
      "name": "Multiplier",
      "direction": "input",
      "type": "int"
    },
    {
      "name": "FinalDamage",
      "direction": "out",
      "type": "float"
    }
  ],
  "local_variables": [
    {
      "name": "TempResult",
      "type": "float",
      "default": "0.0",
      "is_const": false,
      "is_reference": false
    }
  ]
}
```

## Notes

- Returns comprehensive function information including:
  - Function name
  - Total node count in the function graph
  - Graph GUID for internal identification
  - All input and output parameters with their types
  - All local variables with their configuration
- Use this to inspect function structure before modifications
- Parameters show direction (input/out) and types
- Local variables show their type, default value, and modifiers

