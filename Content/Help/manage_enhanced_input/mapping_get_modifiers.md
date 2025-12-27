# mapping_get_modifiers

Get all modifiers on a specific key mapping.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ContextPath | string | Yes | Content path to the Input Mapping Context |
| MappingIndex | number | Yes | Index of the mapping to inspect |

## Examples

### Get Modifiers
```json
{
  "Action": "mapping_get_modifiers",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 0}"
}
```

## Returns

```json
{
  "Success": true,
  "ContextPath": "/Game/Input/IMC_Default",
  "MappingIndex": 0,
  "Modifiers": [
    {
      "Index": 0,
      "Type": "SwizzleInputAxisValues",
      "Settings": {"Order": "YXZ"}
    },
    {
      "Index": 1,
      "Type": "DeadZone",
      "Settings": {"LowerThreshold": 0.2, "UpperThreshold": 1.0}
    }
  ],
  "Count": 2
}
```

## Tips

- Modifiers are applied in index order
- Use Index when removing or modifying specific modifiers
- Settings vary by modifier type
