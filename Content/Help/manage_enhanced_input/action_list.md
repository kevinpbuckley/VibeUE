# action_list

List all Input Action assets in the project.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| Path | string | No | Limit search to a specific content path |

## Examples

### List All Input Actions
```json
{
  "Action": "action_list",
  "ParamsJson": "{}"
}
```

### List Actions in Specific Folder
```json
{
  "Action": "action_list",
  "ParamsJson": "{\"Path\": \"/Game/Input\"}"
}
```

## Returns

```json
{
  "Success": true,
  "Actions": [
    {
      "Path": "/Game/Input/IA_Move",
      "Name": "IA_Move",
      "ValueType": "Axis2D"
    },
    {
      "Path": "/Game/Input/IA_Look",
      "Name": "IA_Look",
      "ValueType": "Axis2D"
    },
    {
      "Path": "/Game/Input/IA_Jump",
      "Name": "IA_Jump",
      "ValueType": "Digital"
    },
    {
      "Path": "/Game/Input/IA_Interact",
      "Name": "IA_Interact",
      "ValueType": "Digital"
    }
  ],
  "Count": 4
}
```

## Tips

- Input Actions are typically organized in an Input folder
- Use this to discover existing actions before creating new ones
- Check ValueType to understand what kind of input each action expects
