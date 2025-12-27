# mapping_remove_modifier

Remove a modifier from a key mapping.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ContextPath | string | Yes | Content path to the Input Mapping Context |
| MappingIndex | number | Yes | Index of the mapping |
| ModifierIndex | number | Yes | Index of the modifier to remove |

## Examples

### Remove Modifier
```json
{
  "Action": "mapping_remove_modifier",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 1, \"ModifierIndex\": 0}"
}
```

## Returns

```json
{
  "Success": true,
  "ContextPath": "/Game/Input/IMC_Default",
  "MappingIndex": 1,
  "RemovedModifierIndex": 0,
  "Message": "Modifier removed successfully"
}
```

## Tips

- Use `mapping_get_modifiers` to find the correct modifier index
- Modifier indices may shift after removal
- Save the context asset after modifications
