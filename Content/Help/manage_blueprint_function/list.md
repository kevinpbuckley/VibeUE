# list

List all functions defined in a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| IncludeInherited | boolean | No | Include inherited functions (default: false) |

## Examples

### List Functions
```json
{
  "Action": "list",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\"}"
}
```

### Include Inherited Functions
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
  "Functions": [
    {
      "Name": "TakeDamage",
      "Category": "Combat",
      "IsPure": false,
      "IsInherited": false,
      "InputCount": 2,
      "OutputCount": 1
    },
    {
      "Name": "GetHealth",
      "Category": "Health",
      "IsPure": true,
      "IsInherited": false,
      "InputCount": 0,
      "OutputCount": 1
    },
    {
      "Name": "CalculateSpeed",
      "Category": "Movement",
      "IsPure": true,
      "IsInherited": false,
      "InputCount": 1,
      "OutputCount": 1
    }
  ]
}
```

## Tips

- Use this to get an overview of available functions in a Blueprint
- Functions are grouped by category when displayed
- Use `get_info` for detailed information about a specific function
- Inherited functions come from parent Blueprint or C++ class
- Event graphs are not included in this list (they are event overrides)
