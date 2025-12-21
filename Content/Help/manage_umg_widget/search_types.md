# search_types

Search for available widget types that can be added.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| SearchTerm | string | No | Search term to filter widget types |
| Category | string | No | Filter by category (e.g., "Common", "Panel", "Input") |

## Examples

### Search All Types
```json
{
  "Action": "search_types",
  "ParamsJson": "{}"
}
```

### Search by Name
```json
{
  "Action": "search_types",
  "ParamsJson": "{\"SearchTerm\": \"Button\"}"
}
```

### Search by Category
```json
{
  "Action": "search_types",
  "ParamsJson": "{\"Category\": \"Input\"}"
}
```

## Returns

```json
{
  "Success": true,
  "WidgetTypes": [
    {
      "Name": "TextBlock",
      "Category": "Common",
      "Description": "Displays text"
    },
    {
      "Name": "Button",
      "Category": "Common",
      "Description": "Clickable button widget"
    },
    {
      "Name": "Image",
      "Category": "Common",
      "Description": "Displays an image or texture"
    },
    {
      "Name": "ProgressBar",
      "Category": "Common",
      "Description": "Shows progress from 0 to 1"
    },
    {
      "Name": "CanvasPanel",
      "Category": "Panel",
      "Description": "Container for absolute positioning"
    },
    {
      "Name": "VerticalBox",
      "Category": "Panel",
      "Description": "Stacks children vertically"
    },
    {
      "Name": "Slider",
      "Category": "Input",
      "Description": "Draggable slider for value selection"
    },
    {
      "Name": "EditableTextBox",
      "Category": "Input",
      "Description": "Text input field"
    }
  ],
  "Count": 8
}
```

## Tips

- Common widgets: TextBlock, Button, Image, ProgressBar
- Panel widgets: CanvasPanel, VerticalBox, HorizontalBox, GridPanel
- Input widgets: Slider, CheckBox, ComboBox, EditableTextBox
- Custom widgets from your project are also listed
