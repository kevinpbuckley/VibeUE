# get_available_events

Get events available for binding on a widget component.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| WidgetPath | string | Yes | Content path to the Widget Blueprint |
| ComponentName | string | Yes | Name of the component to inspect |

## Examples

### Get Button Events
```json
{
  "Action": "get_available_events",
  "ParamsJson": "{\"WidgetPath\": \"/Game/UI/WBP_MainMenu\", \"ComponentName\": \"PlayButton\"}"
}
```

### Get Slider Events
```json
{
  "Action": "get_available_events",
  "ParamsJson": "{\"WidgetPath\": \"/Game/UI/WBP_Options\", \"ComponentName\": \"VolumeSlider\"}"
}
```

## Returns

```json
{
  "Success": true,
  "WidgetPath": "/Game/UI/WBP_MainMenu",
  "ComponentName": "PlayButton",
  "ComponentType": "Button",
  "Events": [
    {
      "Name": "OnClicked",
      "Description": "Called when the button is clicked",
      "Parameters": []
    },
    {
      "Name": "OnPressed",
      "Description": "Called when the button is pressed",
      "Parameters": []
    },
    {
      "Name": "OnReleased",
      "Description": "Called when the button is released",
      "Parameters": []
    },
    {
      "Name": "OnHovered",
      "Description": "Called when mouse enters the button",
      "Parameters": []
    },
    {
      "Name": "OnUnhovered",
      "Description": "Called when mouse leaves the button",
      "Parameters": []
    }
  ]
}
```

## Tips

- Available events depend on widget type
- Buttons: OnClicked, OnPressed, OnReleased, OnHovered, OnUnhovered
- Sliders: OnValueChanged, OnMouseCaptureBegin, OnMouseCaptureEnd
- CheckBoxes: OnCheckStateChanged
- Use bind_events to connect events to functions
