# set_property

Set a property value on an actor instance in the level.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor |
| PropertyName | string | Yes | Name of the property to set |
| Value | any | Yes | New value for the property |

## Examples

### Set Light Intensity
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"ActorName\": \"PointLight_0\", \"PropertyName\": \"Intensity\", \"Value\": 10000}"
}
```

### Set Boolean Property
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"ActorName\": \"BP_Door_0\", \"PropertyName\": \"bIsLocked\", \"Value\": true}"
}
```

### Set Custom Variable
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"ActorName\": \"BP_Enemy_0\", \"PropertyName\": \"MaxHealth\", \"Value\": 200}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "PointLight_0",
  "PropertyName": "Intensity",
  "OldValue": 5000,
  "NewValue": 10000,
  "Message": "Property set successfully"
}
```

## Tips

- Property must be marked as Instance Editable in the Blueprint
- Value type must match the property type
- Changes apply to this instance only, not the Blueprint default
- Save the level to persist property changes
