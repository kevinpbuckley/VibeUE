# action_create

Create a new Input Action asset for the Enhanced Input system.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| action_name | string | Yes | Name of the Input Action (e.g., "IA_Jump", "IA_Move") |
| asset_path | string | Yes | Content folder path where asset will be created (e.g., "/Game/Input/Actions") |
| value_type | string | No | Type of value: "Digital" (bool), "Axis1D" (float), "Axis2D" (Vector2D), "Axis3D" (Vector). Defaults to Axis1D |

## Examples

### Create Digital Action (Button)
```json
{
  "Action": "action_create",
  "ParamsJson": "{\"action_name\": \"IA_Jump\", \"asset_path\": \"/Game/Input/Actions\", \"value_type\": \"Digital\"}"
}
```

### Create 2D Axis Action (Movement)
```json
{
  "Action": "action_create",
  "ParamsJson": "{\"action_name\": \"IA_Move\", \"asset_path\": \"/Game/Input/Actions\", \"value_type\": \"Axis2D\"}"
}
```

### Create 1D Axis Action
```json
{
  "Action": "action_create",
  "ParamsJson": "{\"action_name\": \"IA_Throttle\", \"asset_path\": \"/Game/Input/Actions\", \"value_type\": \"Axis1D\"}"
}
```

### Create 3D Axis Action (Look)
```json
{
  "Action": "action_create",
  "ParamsJson": "{\"action_name\": \"IA_Look\", \"asset_path\": \"/Game/Input/Actions\", \"value_type\": \"Axis3D\"}"
}
```

## Returns

```json
{
  "success": true,
  "action": "action_create",
  "service": "action",
  "message": "Input action 'IA_Jump' created successfully",
  "asset_path": "/Game/Input/Actions/IA_Jump.IA_Jump",
  "usage_hint": "Use this asset_path for mapping_add_key_mapping action_path parameter"
}
```

## Tips

- Use Digital for on/off actions (jump, shoot, interact)
- Use Axis1D for single-axis values (throttle, zoom)
- Use Axis2D for movement and look input (WASD, mouse movement)
- Use Axis3D for full 3D input (VR controllers)
- Naming convention: IA_ prefix for Input Actions
- The returned asset_path includes the full reference path needed for other operations
