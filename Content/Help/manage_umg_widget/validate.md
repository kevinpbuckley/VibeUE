# validate

Validate the structure and bindings of a Widget Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| WidgetPath | string | Yes | Content path to the Widget Blueprint |

## Examples

### Validate Widget
```json
{
  "Action": "validate",
  "ParamsJson": "{\"WidgetPath\": \"/Game/UI/WBP_MainMenu\"}"
}
```

## Returns

```json
{
  "Success": true,
  "WidgetPath": "/Game/UI/WBP_MainMenu",
  "IsValid": true,
  "Warnings": [
    "Button 'PlayButton' has no click event bound"
  ],
  "Errors": []
}
```

## Tips

- Check for unbound events and missing references
- Warnings don't prevent the widget from working
- Errors should be fixed before using the widget
- Run validation after making structural changes
