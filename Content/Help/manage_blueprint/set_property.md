# set_property

Set a default property value on a Blueprint's Class Default Object (CDO).

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| PropertyName | string | Yes | Name of the property to set |
| Value | any | Yes | New value for the property (type must match property type) |

## Examples

### Set Float Property
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Enemy\", \"PropertyName\": \"MaxHealth\", \"Value\": 150.0}"
}
```

### Set Boolean Property
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Item\", \"PropertyName\": \"bCanBePickedUp\", \"Value\": true}"
}
```

### Set String Property
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_NPC\", \"PropertyName\": \"DisplayName\", \"Value\": \"Merchant\"}"
}
```

### Set Vector Property
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Projectile\", \"PropertyName\": \"InitialVelocity\", \"Value\": {\"X\": 0, \"Y\": 0, \"Z\": 1000}}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Enemy",
  "PropertyName": "MaxHealth",
  "OldValue": 100.0,
  "NewValue": 150.0,
  "Message": "Property set successfully"
}
```

## Tips

- Property names are case-sensitive
- Use `get_info` to discover available properties
- Value type must match the property type (float, int, bool, string, struct)
- Changes affect the Class Default Object, not existing instances
- Compile the Blueprint after setting properties to ensure validity
- For component properties, use `manage_blueprint_component` instead
