# mapping_add_key_mapping

Add a key mapping (input binding) to an Input Mapping Context.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ContextPath | string | Yes | Content path to the Input Mapping Context |
| ActionPath | string | Yes | Content path to the Input Action |
| Key | string | Yes | Input key name (e.g., "SpaceBar", "W", "Gamepad_LeftThumbstick") |

## Examples

### Map Jump to Spacebar
```json
{
  "Action": "mapping_add_key_mapping",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"ActionPath\": \"/Game/Input/IA_Jump\", \"Key\": \"SpaceBar\"}"
}
```

### Map Movement to WASD
```json
{
  "Action": "mapping_add_key_mapping",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"ActionPath\": \"/Game/Input/IA_Move\", \"Key\": \"W\"}"
}
```

### Map to Gamepad
```json
{
  "Action": "mapping_add_key_mapping",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"ActionPath\": \"/Game/Input/IA_Jump\", \"Key\": \"Gamepad_FaceButton_Bottom\"}"
}
```

### Map to Mouse Button
```json
{
  "Action": "mapping_add_key_mapping",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"ActionPath\": \"/Game/Input/IA_Fire\", \"Key\": \"LeftMouseButton\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ContextPath": "/Game/Input/IMC_Default",
  "ActionPath": "/Game/Input/IA_Jump",
  "Key": "SpaceBar",
  "MappingIndex": 0,
  "Message": "Key mapping added successfully"
}
```

## Tips

- Use `mapping_get_available_keys` to see all valid key names
- Multiple keys can map to the same action (for keyboard + gamepad support)
- MappingIndex is used to reference this mapping when adding modifiers/triggers
- Common keys: SpaceBar, LeftMouseButton, RightMouseButton, W, A, S, D
- Gamepad keys: Gamepad_LeftThumbstick, Gamepad_RightThumbstick, Gamepad_FaceButton_Bottom
