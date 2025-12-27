# get_component_properties

Get all properties of a specific widget component. **Both `widget_name` AND `component_name` are required.**

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| widget_name | string | **Yes** | Content path to the Widget Blueprint |
| component_name | string | **Yes** | Name of the component to inspect |

## ⚠️ IMPORTANT

- **Both parameters are required** - you must specify which component to inspect
- Use `list_components` first to get valid component names
- If `list_components` returns empty `[]`, the widget has NO components yet

## Examples

### Get Text Block Properties
```json
{
  "Action": "get_component_properties",
  "ParamsJson": "{\"widget_name\": \"/Game/UI/WBP_MainMenu\", \"component_name\": \"TitleText\"}"
}
```

### Workflow - Always List First
```json
// Step 1: List components to find valid names
{"Action": "list_components", "ParamsJson": "{\"widget_name\": \"/Game/UI/WBP_MainMenu\"}"}
// Returns: {"components": [{"name": "RootCanvas"}, {"name": "TitleText"}]}

// Step 2: Now get properties for a specific component
{"Action": "get_component_properties", "ParamsJson": "{\"widget_name\": \"/Game/UI/WBP_MainMenu\", \"component_name\": \"TitleText\"}"}
```

## Returns

```json
{
  "Success": true,
  "WidgetPath": "/Game/UI/WBP_MainMenu",
  "ComponentName": "TitleText",
  "ComponentType": "TextBlock",
  "Properties": [
    {
      "Name": "Text",
      "Type": "Text",
      "Value": "Main Menu",
      "Category": "Content"
    },
    {
      "Name": "Font",
      "Type": "SlateFontInfo",
      "Category": "Appearance"
    },
    {
      "Name": "ColorAndOpacity",
      "Type": "SlateColor",
      "Value": {"R": 1, "G": 1, "B": 1, "A": 1},
      "Category": "Appearance"
    },
    {
      "Name": "Justification",
      "Type": "ETextJustify",
      "Value": "Left",
      "Category": "Appearance"
    },
    {
      "Name": "Visibility",
      "Type": "ESlateVisibility",
      "Value": "Visible",
      "Category": "Behavior"
    }
  ]
}
```

## Tips

- Properties vary by widget type
- Use this to discover property names before setting values
- Common properties: Visibility, RenderOpacity, IsEnabled
- Text widgets have: Text, Font, ColorAndOpacity, Justification
