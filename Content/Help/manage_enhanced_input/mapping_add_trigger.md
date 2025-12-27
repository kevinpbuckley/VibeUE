# mapping_add_trigger

Add a trigger to a key mapping to control when the action fires.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ContextPath | string | Yes | Content path to the Input Mapping Context |
| MappingIndex | number | Yes | Index of the mapping to modify |
| TriggerType | string | Yes | Type of trigger to add |
| Settings | object | No | Configuration settings for the trigger |

## Examples

### Add Pressed Trigger
```json
{
  "Action": "mapping_add_trigger",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 0, \"TriggerType\": \"Pressed\"}"
}
```

### Add Released Trigger
```json
{
  "Action": "mapping_add_trigger",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 0, \"TriggerType\": \"Released\"}"
}
```

### Add Hold Trigger
```json
{
  "Action": "mapping_add_trigger",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 2, \"TriggerType\": \"Hold\", \"Settings\": {\"HoldTimeThreshold\": 0.5}}"
}
```

### Add Tap Trigger
```json
{
  "Action": "mapping_add_trigger",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 0, \"TriggerType\": \"Tap\", \"Settings\": {\"TapReleaseTimeThreshold\": 0.2}}"
}
```

### Add Chorded Action Trigger
```json
{
  "Action": "mapping_add_trigger",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 0, \"TriggerType\": \"ChordAction\", \"Settings\": {\"ChordAction\": \"/Game/Input/IA_Sprint\"}}"
}
```

## Returns

```json
{
  "Success": true,
  "ContextPath": "/Game/Input/IMC_Default",
  "MappingIndex": 0,
  "TriggerType": "Pressed",
  "Message": "Trigger added successfully"
}
```

## Tips

- Use `mapping_get_available_trigger_types` to see all trigger types
- Common triggers: Pressed, Released, Hold, Tap, Down
- Pressed fires once when input starts
- Released fires once when input ends
- Hold requires input to be held for a duration
- Tap requires quick press and release
- ChordAction requires another action to be active (like Shift+Key)
