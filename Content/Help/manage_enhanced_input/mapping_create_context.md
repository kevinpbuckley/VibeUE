# mapping_create_context

Create a new Input Mapping Context asset.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| Path | string | Yes | Content path for the new Input Mapping Context |
| Description | string | No | Description of the mapping context's purpose |

## Examples

### Create Default Context
```json
{
  "Action": "mapping_create_context",
  "ParamsJson": "{\"Path\": \"/Game/Input/IMC_Default\", \"Description\": \"Default gameplay input mappings\"}"
}
```

### Create Vehicle Context
```json
{
  "Action": "mapping_create_context",
  "ParamsJson": "{\"Path\": \"/Game/Input/IMC_Vehicle\", \"Description\": \"Input mappings for vehicle control\"}"
}
```

### Create Menu Context
```json
{
  "Action": "mapping_create_context",
  "ParamsJson": "{\"Path\": \"/Game/Input/IMC_Menu\", \"Description\": \"Input mappings for menu navigation\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ContextPath": "/Game/Input/IMC_Default",
  "Message": "Input Mapping Context created successfully"
}
```

## Tips

- Naming convention: IMC_ prefix for Input Mapping Contexts
- Create separate contexts for different gameplay modes
- Contexts can be added/removed at runtime to change input schemes
- Higher priority contexts override lower priority ones
