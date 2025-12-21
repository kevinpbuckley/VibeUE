# action_configure

Configure settings on an Input Action.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActionPath | string | Yes | Content path to the Input Action |
| ConsumeInput | boolean | No | Whether to consume input and prevent propagation |
| TriggerWhenPaused | boolean | No | Whether to trigger during game pause |
| Description | string | No | Update the description |

## Examples

### Configure Action to Not Consume Input
```json
{
  "Action": "action_configure",
  "ParamsJson": "{\"ActionPath\": \"/Game/Input/IA_Menu\", \"ConsumeInput\": false}"
}
```

### Allow Action During Pause
```json
{
  "Action": "action_configure",
  "ParamsJson": "{\"ActionPath\": \"/Game/Input/IA_Pause\", \"TriggerWhenPaused\": true}"
}
```

### Update Description
```json
{
  "Action": "action_configure",
  "ParamsJson": "{\"ActionPath\": \"/Game/Input/IA_Fire\", \"Description\": \"Primary fire action for weapons\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActionPath": "/Game/Input/IA_Menu",
  "ModifiedSettings": ["ConsumeInput"],
  "Message": "Input Action configured successfully"
}
```

## Tips

- ConsumeInput=false allows multiple actions to respond to the same key
- TriggerWhenPaused is essential for pause menu input
- Save the asset after configuration changes
