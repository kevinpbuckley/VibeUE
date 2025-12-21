# mapping_remove_trigger

Remove a trigger from a key mapping.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ContextPath | string | Yes | Content path to the Input Mapping Context |
| MappingIndex | number | Yes | Index of the mapping |
| TriggerIndex | number | Yes | Index of the trigger to remove |

## Examples

### Remove Trigger
```json
{
  "Action": "mapping_remove_trigger",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 0, \"TriggerIndex\": 0}"
}
```

## Returns

```json
{
  "Success": true,
  "ContextPath": "/Game/Input/IMC_Default",
  "MappingIndex": 0,
  "RemovedTriggerIndex": 0,
  "Message": "Trigger removed successfully"
}
```

## Tips

- Use `mapping_get_triggers` to find the correct trigger index
- Trigger indices may shift after removal
- Mapping without triggers uses default behavior (fires continuously while held)
