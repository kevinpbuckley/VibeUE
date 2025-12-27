# search_types

Search for available variable types that can be used in Blueprints.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| SearchTerm | string | No | Search term to filter types |
| Category | string | No | Filter by category (e.g., "Basic", "Structure", "Object") |

## Examples

### Search All Types
```json
{
  "Action": "search_types",
  "ParamsJson": "{}"
}
```

### Search for Vector Types
```json
{
  "Action": "search_types",
  "ParamsJson": "{\"SearchTerm\": \"Vector\"}"
}
```

### Search Object Types
```json
{
  "Action": "search_types",
  "ParamsJson": "{\"Category\": \"Object\"}"
}
```

## Returns

```json
{
  "Success": true,
  "Types": [
    {
      "Name": "Vector",
      "Category": "Structure",
      "Description": "A 3D vector (X, Y, Z)"
    },
    {
      "Name": "Vector2D",
      "Category": "Structure",
      "Description": "A 2D vector (X, Y)"
    },
    {
      "Name": "Vector4",
      "Category": "Structure",
      "Description": "A 4D vector (X, Y, Z, W)"
    }
  ],
  "Count": 3
}
```

## Tips

- Basic types: Boolean, Byte, Integer, Integer64, Float, Double, Name, String, Text
- Structure types: Vector, Rotator, Transform, Color, LinearColor
- Object types: Actor, Object, Class references
- Use this to discover the exact type name before creating variables
- Custom structs and enums from your project are also listed
