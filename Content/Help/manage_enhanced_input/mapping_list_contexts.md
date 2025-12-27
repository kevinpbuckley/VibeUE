# mapping_list_contexts

List all Input Mapping Context assets in the project.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| Path | string | No | Limit search to a specific content path |

## Examples

### List All Contexts
```json
{
  "Action": "mapping_list_contexts",
  "ParamsJson": "{}"
}
```

### List Contexts in Folder
```json
{
  "Action": "mapping_list_contexts",
  "ParamsJson": "{\"Path\": \"/Game/Input\"}"
}
```

## Returns

```json
{
  "Success": true,
  "Contexts": [
    {
      "Path": "/Game/Input/IMC_Default",
      "Name": "IMC_Default",
      "MappingCount": 8
    },
    {
      "Path": "/Game/Input/IMC_Vehicle",
      "Name": "IMC_Vehicle",
      "MappingCount": 5
    },
    {
      "Path": "/Game/Input/IMC_Menu",
      "Name": "IMC_Menu",
      "MappingCount": 3
    }
  ],
  "Count": 3
}
```

## Tips

- MappingCount shows how many key bindings are in each context
- Use this to discover existing contexts before adding mappings
- Contexts are typically organized in an Input folder
