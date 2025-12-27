# mapping_remove_mapping

Remove a key mapping from an Input Mapping Context.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ContextPath | string | Yes | Content path to the Input Mapping Context |
| MappingIndex | number | Yes | Index of the mapping to remove (from mapping_get_mappings) |

## Examples

### Remove Mapping by Index
```json
{
  "Action": "mapping_remove_mapping",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"MappingIndex\": 2}"
}
```

## Returns

```json
{
  "Success": true,
  "ContextPath": "/Game/Input/IMC_Default",
  "RemovedIndex": 2,
  "Message": "Key mapping removed successfully"
}
```

## Tips

- Use `mapping_get_mappings` to find the correct index first
- Indices may shift after removal, so refresh mapping list
- Removing a mapping removes all its modifiers and triggers
- Save the context asset after modifications
