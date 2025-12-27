# get_property

Get the current value of a default property from a Blueprint's Class Default Object.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| PropertyName | string | Yes | Name of the property to retrieve |

## Examples

### Get Float Property
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Enemy\", \"PropertyName\": \"MaxHealth\"}"
}
```

### Get Boolean Property
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Door\", \"PropertyName\": \"bIsLocked\"}"
}
```

### Get Struct Property
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Weapon\", \"PropertyName\": \"DamageRange\"}"
}
```

## Returns

### Simple Value
```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Enemy",
  "PropertyName": "MaxHealth",
  "PropertyType": "Float",
  "Value": 100.0
}
```

### Struct Value
```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Weapon",
  "PropertyName": "DamageRange",
  "PropertyType": "Vector2D",
  "Value": {
    "X": 10.0,
    "Y": 25.0
  }
}
```

## Tips

- Property names are case-sensitive
- Use `get_info` to list all available properties
- Returns the Class Default Object value, not instance values
- Complex types (arrays, maps, structs) are returned as JSON objects
- Inherited properties from the parent class are also accessible
