# set_property

Set a property value on a specific component within a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| ComponentName | string | Yes | Name of the component to modify |
| PropertyName | string | Yes | Name of the property to set |
| Value | any | Yes | New value for the property |

## Examples

### Set Mesh Asset
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Pickup\", \"ComponentName\": \"PickupMesh\", \"PropertyName\": \"StaticMesh\", \"Value\": \"/Game/Meshes/SM_Coin\"}"
}
```

### Set Light Intensity
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Lamp\", \"ComponentName\": \"PointLight\", \"PropertyName\": \"Intensity\", \"Value\": 10000}"
}
```

### Set Relative Location
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Weapon\", \"ComponentName\": \"MuzzlePoint\", \"PropertyName\": \"RelativeLocation\", \"Value\": {\"X\": 100, \"Y\": 0, \"Z\": 5}}"
}
```

### Set Collision Enabled
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Trigger\", \"ComponentName\": \"TriggerBox\", \"PropertyName\": \"CollisionEnabled\", \"Value\": \"QueryOnly\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Lamp",
  "ComponentName": "PointLight",
  "PropertyName": "Intensity",
  "OldValue": 5000,
  "NewValue": 10000,
  "Message": "Property set successfully"
}
```

## Tips

- Property names are case-sensitive
- Use `get_all_properties` to discover available properties on a component
- Transform properties: RelativeLocation, RelativeRotation, RelativeScale3D
- Asset references should be full content paths
- Compile the Blueprint after property changes
