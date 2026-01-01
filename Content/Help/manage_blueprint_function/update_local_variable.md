# update_local_variable

Update properties of an existing local variable in a blueprint function.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| blueprint_name | string | Yes | Name or path of the blueprint |
| function_name | string | Yes | Name of the function containing the local variable |
| local_name | string | Yes | Name of the local variable to update (also accepts variable_name or param_name) |
| type | string | No | New type for the local variable (e.g., "float", "int", "Vector") |
| default_value | string | No | New default value for the local variable |
| category | string | No | New category for organization |

## Returns

```json
{
  "success": true,
  "updated_local": "TempResult",
  "locals": [
    {
      "name": "TempResult",
      "friendly_name": "Temp Result",
      "type": "int",
      "display_type": "Integer",
      "default_value": "0",
      "category": "Default",
      "pin_category": "int",
      "guid": "GUID_STRING",
      "is_const": false,
      "is_reference": false,
      "is_editable": true,
      "is_array": false,
      "is_set": false,
      "is_map": false
    }
  ],
  "count": 1
}
```

## Example Usage

### Change Type
```json
{
  "Action": "update_local_variable",
  "ParamsJson": {
    "blueprint_name": "BP_Player",
    "function_name": "CalculateHealth",
    "local_name": "TempResult",
    "type": "int"
  }
}
```

### Change Default Value
```json
{
  "Action": "update_local_variable",
  "ParamsJson": {
    "blueprint_name": "BP_Player",
    "function_name": "CalculateHealth",
    "local_name": "Counter",
    "default_value": "10"
  }
}
```

### Change Multiple Properties
```json
{
  "Action": "update_local_variable",
  "ParamsJson": {
    "blueprint_name": "BP_Player",
    "function_name": "CalculateHealth",
    "local_name": "TempResult",
    "type": "Vector",
    "default_value": "0,0,0",
    "category": "Calculations"
  }
}
```

## Notes

- Only the specified properties will be updated; other properties remain unchanged
- Changing type may clear the default value if incompatible
- The variable must exist in the function; use add_local_variable to create new variables
- After updating, compile the blueprint to apply changes
