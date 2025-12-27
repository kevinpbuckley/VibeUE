# mapping_add_modifier

Add a modifier to a key mapping to transform the input value.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ContextPath | string | Yes | Content path to the Input Mapping Context |
| MappingIndex | number | Yes | Index of the mapping to modify |
| ModifierType | string | Yes | Type of modifier to add |
| Settings | object | No | Configuration settings for the modifier |

## Examples

### Add Negate Modifier
```json
{
  "Action": "mapping_add_modifier",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 1, \"ModifierType\": \"Negate\"}"
}
```

### Add Swizzle Modifier
```json
{
  "Action": "mapping_add_modifier",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 0, \"ModifierType\": \"SwizzleInputAxisValues\", \"Settings\": {\"Order\": \"YXZ\"}}"
}
```

### Add Dead Zone Modifier
```json
{
  "Action": "mapping_add_modifier",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 0, \"ModifierType\": \"DeadZone\", \"Settings\": {\"LowerThreshold\": 0.2, \"UpperThreshold\": 1.0}}"
}
```

### Add Scalar Modifier
```json
{
  "Action": "mapping_add_modifier",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 0, \"ModifierType\": \"Scalar\", \"Settings\": {\"Scalar\": {\"X\": 2.0, \"Y\": 2.0, \"Z\": 1.0}}}"
}
```

## Returns

```json
{
  "Success": true,
  "ContextPath": "/Game/Input/IMC_Default",
  "MappingIndex": 1,
  "ModifierType": "Negate",
  "Message": "Modifier added successfully"
}
```

## Tips

- Use `mapping_get_available_modifier_types` to see all modifier types
- Modifiers are applied in order - sequence matters
- Common modifiers: Negate, Swizzle, DeadZone, Scalar, Smooth
- Negate inverts the input value (e.g., S key for backward movement)
- Swizzle reorders axis components (e.g., convert X to Y for WASD)
