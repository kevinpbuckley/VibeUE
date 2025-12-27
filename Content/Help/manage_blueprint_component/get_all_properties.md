# get_all_properties

Get all editable properties of a component.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| ComponentName | string | Yes | Name of the component to inspect |

## Examples

### Get All Properties
```json
{
  "Action": "get_all_properties",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Lamp\", \"ComponentName\": \"PointLight\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Lamp",
  "ComponentName": "PointLight",
  "ComponentClass": "PointLightComponent",
  "Properties": [
    {
      "Name": "Intensity",
      "Type": "Float",
      "Value": 5000,
      "Category": "Light"
    },
    {
      "Name": "LightColor",
      "Type": "Color",
      "Value": {"R": 255, "G": 255, "B": 255, "A": 255},
      "Category": "Light"
    },
    {
      "Name": "AttenuationRadius",
      "Type": "Float",
      "Value": 1000,
      "Category": "Light"
    },
    {
      "Name": "CastShadows",
      "Type": "Boolean",
      "Value": true,
      "Category": "Light"
    },
    {
      "Name": "RelativeLocation",
      "Type": "Vector",
      "Value": {"X": 0, "Y": 0, "Z": 100},
      "Category": "Transform"
    }
  ]
}
```

## Tips

- Properties are grouped by category for easier navigation
- This is useful for discovering what can be configured on a component
- Not all properties may be editable at runtime
- Use this to find the exact property names before using `set_property`
