# get_all_properties

Get all editable properties of an actor instance.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor |
| Category | string | No | Filter by property category |

## Examples

### Get All Properties
```json
{
  "Action": "get_all_properties",
  "ParamsJson": "{\"ActorName\": \"BP_Enemy_0\"}"
}
```

### Get Properties by Category
```json
{
  "Action": "get_all_properties",
  "ParamsJson": "{\"ActorName\": \"PointLight_0\", \"Category\": \"Light\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ActorName": "BP_Enemy_0",
  "Properties": [
    {
      "Name": "Health",
      "Type": "Float",
      "Value": 100,
      "Category": "Stats"
    },
    {
      "Name": "MaxHealth",
      "Type": "Float",
      "Value": 100,
      "Category": "Stats"
    },
    {
      "Name": "MovementSpeed",
      "Type": "Float",
      "Value": 300,
      "Category": "Movement"
    },
    {
      "Name": "bIsAggressive",
      "Type": "Boolean",
      "Value": false,
      "Category": "AI"
    }
  ],
  "Count": 4
}
```

## Tips

- Only shows Instance Editable properties
- Use this to discover property names before get/set
- Properties are grouped by category
- Native and Blueprint-defined properties are included
