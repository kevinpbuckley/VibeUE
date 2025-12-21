# get_property

Get a property value from an actor instance in the level.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor |
| PropertyName | string | Yes | Name of the property to retrieve |

## Examples

### Get Light Intensity
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"ActorName\": \"PointLight_0\", \"PropertyName\": \"Intensity\"}"
}
```

### Get Custom Variable
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"ActorName\": \"BP_Enemy_0\", \"PropertyName\": \"Health\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "PointLight_0",
  "PropertyName": "Intensity",
  "PropertyType": "Float",
  "Value": 5000
}
```

## Tips

- Property names are case-sensitive
- Works for both native and Blueprint-defined properties
- Use `get_all_properties` to discover available properties
- Instance values may differ from Blueprint defaults
