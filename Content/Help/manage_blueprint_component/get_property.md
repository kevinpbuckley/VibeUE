# get_property

Get the current value of a property from a component.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| ComponentName | string | Yes | Name of the component |
| PropertyName | string | Yes | Name of the property to retrieve |

## Examples

### Get Mesh Reference
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Pickup\", \"ComponentName\": \"PickupMesh\", \"PropertyName\": \"StaticMesh\"}"
}
```

### Get Light Color
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Lamp\", \"ComponentName\": \"PointLight\", \"PropertyName\": \"LightColor\"}"
}
```

### Get Relative Transform
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Actor\", \"ComponentName\": \"Mesh\", \"PropertyName\": \"RelativeLocation\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Lamp",
  "ComponentName": "PointLight",
  "PropertyName": "LightColor",
  "PropertyType": "Color",
  "Value": {
    "R": 255,
    "G": 200,
    "B": 150,
    "A": 255
  }
}
```

## Tips

- Use `get_all_properties` if you're unsure of the exact property name
- Property names are case-sensitive
- Complex values (vectors, colors, transforms) are returned as objects
- Asset references are returned as content paths
