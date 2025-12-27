# get_property

Get a specific property value from a widget component.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| WidgetPath | string | Yes | Content path to the Widget Blueprint |
| ComponentName | string | Yes | Name of the component |
| PropertyName | string | Yes | Name of the property to retrieve |

## Examples

### Get Text Value
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"WidgetPath\": \"/Game/UI/WBP_MainMenu\", \"ComponentName\": \"TitleText\", \"PropertyName\": \"Text\"}"
}
```

### Get Visibility
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"WidgetPath\": \"/Game/UI/WBP_HUD\", \"ComponentName\": \"DeathScreen\", \"PropertyName\": \"Visibility\"}"
}
```

## Returns

```json
{
  "Success": true,
  "WidgetPath": "/Game/UI/WBP_MainMenu",
  "ComponentName": "TitleText",
  "PropertyName": "Text",
  "PropertyType": "Text",
  "Value": "Main Menu"
}
```

## Tips

- Property names are case-sensitive
- Use `get_component_properties` to discover available properties
- Text values are localized text, not plain strings
