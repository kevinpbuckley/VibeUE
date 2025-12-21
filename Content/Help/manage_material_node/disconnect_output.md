# disconnect_output

Disconnect a material output property.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| material_path | string | Yes | Full path to the material |
| material_property | string | Yes | Material output to disconnect: BaseColor, Roughness, etc. |

## Examples

```json
{
  "action": "disconnect_output",
  "material_path": "/Game/Materials/M_Test",
  "material_property": "BaseColor"
}
```

## Returns

- `success`: true/false
- `message`: Confirmation message
- `disconnected_property`: Property that was disconnected

## Tips

- This removes the connection TO the material output, not from expressions
- The expression node remains, just disconnected from output
- Use to rewire material outputs to different expressions
