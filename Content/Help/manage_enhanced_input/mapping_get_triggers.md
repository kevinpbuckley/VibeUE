# mapping_get_triggers

Get all triggers on a specific key mapping.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ContextPath | string | Yes | Content path to the Input Mapping Context |
| MappingIndex | number | Yes | Index of the mapping to inspect |

## Examples

### Get Triggers
```json
{
  "Action": "mapping_get_triggers",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 2}"
}
```

## Returns

```json
{
  "Success": true,
  "ContextPath": "/Game/Input/IMC_Default",
  "MappingIndex": 2,
  "Triggers": [
    {
      "Index": 0,
      "Type": "Pressed"
    },
    {
      "Index": 1,
      "Type": "Hold",
      "Settings": {"HoldTimeThreshold": 1.0}
    }
  ],
  "Count": 2
}
```

## Tips

- Multiple triggers can exist on a single mapping
- Triggers determine when Started, Triggered, and Completed events fire
- Use Index when removing specific triggers
