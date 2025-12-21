# list

List all variables defined in a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| IncludeInherited | boolean | No | Include inherited variables from parent class (default: false) |

## Examples

### List Blueprint Variables
```json
{
  "Action": "list",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\"}"
}
```

### Include Inherited Variables
```json
{
  "Action": "list",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"IncludeInherited\": true}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "Variables": [
    {
      "Name": "Health",
      "Type": "Float",
      "Category": "Stats",
      "IsInstanceEditable": true,
      "IsInherited": false
    },
    {
      "Name": "MaxHealth",
      "Type": "Float",
      "Category": "Stats",
      "IsInstanceEditable": true,
      "IsInherited": false
    },
    {
      "Name": "MovementSpeed",
      "Type": "Float",
      "Category": "Movement",
      "IsInstanceEditable": true,
      "IsInherited": false
    },
    {
      "Name": "Inventory",
      "Type": "Array<Object>",
      "Category": "Items",
      "IsInstanceEditable": false,
      "IsInherited": false
    }
  ],
  "Count": 4
}
```

## Tips

- Variables are grouped by category in the Blueprint editor
- Instance editable variables can be customized per actor in the level
- Inherited variables come from parent Blueprints or C++ classes
- Use `get_info` for detailed information about specific variables
