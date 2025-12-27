# mapping_get_available_keys

Get a list of all available input keys that can be used in mappings.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| Category | string | No | Filter by category: "Keyboard", "Mouse", "Gamepad", "Touch" |

## Examples

### Get All Keys
```json
{
  "Action": "mapping_get_available_keys",
  "ParamsJson": "{}"
}
```

### Get Keyboard Keys
```json
{
  "Action": "mapping_get_available_keys",
  "ParamsJson": "{\"Category\": \"Keyboard\"}"
}
```

### Get Gamepad Keys
```json
{
  "Action": "mapping_get_available_keys",
  "ParamsJson": "{\"Category\": \"Gamepad\"}"
}
```

## Returns

```json
{
  "Success": true,
  "Keys": [
    {"Name": "SpaceBar", "Category": "Keyboard"},
    {"Name": "W", "Category": "Keyboard"},
    {"Name": "A", "Category": "Keyboard"},
    {"Name": "S", "Category": "Keyboard"},
    {"Name": "D", "Category": "Keyboard"},
    {"Name": "LeftMouseButton", "Category": "Mouse"},
    {"Name": "RightMouseButton", "Category": "Mouse"},
    {"Name": "MouseX", "Category": "Mouse"},
    {"Name": "MouseY", "Category": "Mouse"},
    {"Name": "Gamepad_LeftThumbstick", "Category": "Gamepad"},
    {"Name": "Gamepad_RightThumbstick", "Category": "Gamepad"},
    {"Name": "Gamepad_FaceButton_Bottom", "Category": "Gamepad"}
  ],
  "Count": 12
}
```

## Tips

- Key names are case-sensitive
- Gamepad face buttons: Gamepad_FaceButton_Bottom (A/X), _Right (B/O), _Top (Y/Triangle), _Left (X/Square)
- Mouse axes: MouseX, MouseY, MouseWheelAxis
- Gamepad thumbsticks return axis values, buttons return digital
