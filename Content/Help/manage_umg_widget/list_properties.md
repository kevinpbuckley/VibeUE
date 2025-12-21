# list_properties

List all editable properties for a widget component.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| widget_name | string | Yes | Content path to the Widget Blueprint |
| component_name | string | Yes | Name of the component to list properties for |

## Examples

### List Button Properties
```python
manage_umg_widget(action="list_properties", widget_name="/Game/UI/WBP_MainMenu",
                  component_name="PlayButton")
```

### List TextBlock Properties
```python
manage_umg_widget(action="list_properties", widget_name="/Game/UI/WBP_MainMenu",
                  component_name="TitleText")
```

## Returns

```json
{
  "success": true,
  "widget_name": "/Game/UI/WBP_MainMenu",
  "component_name": "PlayButton",
  "properties": [
    {
      "name": "ColorAndOpacity",
      "type": "FLinearColor",
      "value": "(R=1.0,G=1.0,B=1.0,A=1.0)",
      "category": "Appearance",
      "editable": true
    },
    {
      "name": "Slot.HorizontalAlignment",
      "type": "EHorizontalAlignment",
      "value": "HAlign_Fill",
      "category": "Slot",
      "editable": true
    }
    // ... more properties
  ],
  "count": 41,
  "include_slot_properties": true
}
```

## Tips

- Use this to discover available properties before calling `set_property`
- Properties include slot properties for layout control (prefixed with "Slot.")
- Check "editable" field - some properties cannot be modified
- Event delegate properties (OnClicked, etc.) have editable=false - use `bind_events` instead
