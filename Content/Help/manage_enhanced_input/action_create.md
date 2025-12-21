# action_create

Create a new Input Action asset for the Enhanced Input system.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| Path | string | Yes | Content path for the new Input Action |
| ValueType | string | Yes | Type of value: "Digital" (bool), "Axis1D" (float), "Axis2D" (Vector2D), "Axis3D" (Vector) |
| Description | string | No | Description of what this action represents |

## Examples

### Create Digital Action (Button)
```json
{
  "Action": "action_create",
  "ParamsJson": "{\"Path\": \"/Game/Input/IA_Jump\", \"ValueType\": \"Digital\", \"Description\": \"Jump action\"}"
}
```

### Create 2D Axis Action (Movement)
```json
{
  "Action": "action_create",
  "ParamsJson": "{\"Path\": \"/Game/Input/IA_Move\", \"ValueType\": \"Axis2D\", \"Description\": \"Character movement\"}"
}
```

### Create 1D Axis Action
```json
{
  "Action": "action_create",
  "ParamsJson": "{\"Path\": \"/Game/Input/IA_Throttle\", \"ValueType\": \"Axis1D\", \"Description\": \"Vehicle throttle\"}"
}
```

### Create 3D Axis Action (Look)
```json
{
  "Action": "action_create",
  "ParamsJson": "{\"Path\": \"/Game/Input/IA_Look\", \"ValueType\": \"Axis3D\", \"Description\": \"Camera look direction\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActionPath": "/Game/Input/IA_Jump",
  "ValueType": "Digital",
  "Message": "Input Action created successfully"
}
```

## Tips

- Use Digital for on/off actions (jump, shoot, interact)
- Use Axis1D for single-axis values (throttle, zoom)
- Use Axis2D for movement and look input (WASD, mouse movement)
- Use Axis3D for full 3D input (VR controllers)
- Naming convention: IA_ prefix for Input Actions
