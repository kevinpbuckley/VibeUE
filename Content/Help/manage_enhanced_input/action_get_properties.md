# action_get_properties

Get the properties and configuration of an Input Action.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| action_path | string | Yes | Content path to the Input Action (aliases: ActionPath, asset_path) |

## Examples

### Get Action Properties
```json
{
  "Action": "action_get_properties",
  "ParamsJson": "{\"action_path\": \"/Game/Input/IA_Move\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActionPath": "/Game/Input/IA_Move",
  "Properties": {
    "ValueType": "Axis2D",
    "Description": "Character movement input",
    "bConsumeInput": true,
    "bTriggerWhenPaused": false,
    "Triggers": [],
    "Modifiers": []
  }
}
```

## Tips

- ValueType determines the data type of the action value
- bConsumeInput prevents the input from propagating to other actions
- bTriggerWhenPaused allows the action during game pause
- Action-level modifiers and triggers apply to all mappings
