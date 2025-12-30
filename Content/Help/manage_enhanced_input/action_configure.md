# action_configure

Configure settings on an Input Action.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| action_path | string | Yes | Content path to the Input Action (aliases: ActionPath, asset_path) |
| ConsumeInput | boolean | No | Whether to consume input and prevent propagation |
| TriggerWhenPaused | boolean | No | Whether to trigger during game pause |
| Description | string | No | Update the description |

## Examples

### Configure Action to Not Consume Input
```json
{
  "Action": "action_configure",
  "ParamsJson": "{\"action_path\": \"/Game/Input/IA_Menu\", \"ConsumeInput\": false}"
}
```

### Allow Action During Pause
```json
{
  "Action": "action_configure",
  "ParamsJson": "{\"action_path\": \"/Game/Input/IA_Pause\", \"TriggerWhenPaused\": true}"
}
```

### Update Description
```json
{
  "Action": "action_configure",
  "ParamsJson": "{\"action_path\": \"/Game/Input/IA_Fire\", \"Description\": \"Primary fire action for weapons\"}"
}
```

### Set Multiple Properties
```json
{
  "Action": "action_configure",
  "ParamsJson": "{\"action_path\": \"/Game/Input/IA_Interact\", \"ConsumeInput\": true, \"Description\": \"Player object interaction\"}"
}
```

## Returns

```json
{
  "Success": true,
  "action_path": "/Game/Input/IA_Menu",
  "modified_settings": ["bConsumeInput"],
  "message": "Configured 1 property(ies) on Input Action"
}
```

## Tips

- ConsumeInput=false allows multiple actions to respond to the same key
- TriggerWhenPaused is essential for pause menu input
- Save the asset after configuration changes
