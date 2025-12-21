# reflection_discover_types

Discover available Enhanced Input types including actions, contexts, modifiers, and triggers.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| TypeCategory | string | No | Category to discover: "Actions", "Contexts", "Modifiers", "Triggers", "All" |

## Examples

### Discover All Types
```json
{
  "Action": "reflection_discover_types",
  "ParamsJson": "{}"
}
```

### Discover Only Actions
```json
{
  "Action": "reflection_discover_types",
  "ParamsJson": "{\"TypeCategory\": \"Actions\"}"
}
```

### Discover Modifiers
```json
{
  "Action": "reflection_discover_types",
  "ParamsJson": "{\"TypeCategory\": \"Modifiers\"}"
}
```

## Returns

```json
{
  "Success": true,
  "Discovery": {
    "Actions": {
      "ValueTypes": ["Digital", "Axis1D", "Axis2D", "Axis3D"],
      "Description": "Input Actions define the type and behavior of input"
    },
    "Contexts": {
      "Description": "Input Mapping Contexts group related input bindings"
    },
    "Modifiers": {
      "Types": ["Negate", "DeadZone", "Scalar", "Swizzle", "Smooth", "ResponseCurve"],
      "Description": "Modifiers transform input values before processing"
    },
    "Triggers": {
      "Types": ["Down", "Pressed", "Released", "Hold", "Tap", "Pulse", "Chord"],
      "Description": "Triggers determine when input events fire"
    }
  }
}
```

## Tips

- Use this for an overview of the Enhanced Input system
- Helpful for understanding available options before setup
- TypeCategory filters the response to specific areas
- Combine with action-specific discovery tools for detailed info
